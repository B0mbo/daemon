//JSONService.h
//Author: Bombo
//07.02.2017
//Класс JSONService обеспечивает формирование JSON запросов на обновление записей в базе данных.

#include"JSONService.h"

using namespace undefinedspace;

/***********************************JSONService*************************************/

JSONService::JSONService()
{
  stType = NOT_READY;
  ulSessionNumber = -1;
  pfscFirst = NULL;
  pjsNext = NULL;
  mJSONServiceMutex = PTHREAD_MUTEX_INITIALIZER;
}

JSONService::JSONService(ServiceType in_stType, unsigned long in_ulSessionNumber)
{
  stType = in_stType;
  ulSessionNumber = in_ulSessionNumber;
  pfscFirst = NULL;
  pjsNext = NULL;
  mJSONServiceMutex = PTHREAD_MUTEX_INITIALIZER;
}

JSONService::~JSONService()
{
  FSChange *pfscList, *pfscDel;

  //возможно, при удалении имеет смысл попытаться отправить список через сокет (?)
  pthread_mutex_lock(&mJSONServiceMutex);
  pfscList = pfscDel = pfscFirst;
  while(pfscList != NULL)
  {
    pfscList = pfscList->pfscNext;
    delete pfscDel;
    pfscDel = pfscList;
  }
  pthread_mutex_unlock(&mJSONServiceMutex);

  pthread_mutex_destroy(&mJSONServiceMutex);
}

char * const JSONService::GetJSON(void)
{
  char *pszRet = NULL;
  size_t stLen;
  FSChange *pfscList, *pfscLast;

  if(pfscFirst == NULL)
    return NULL;

  pthread_mutex_lock(&mJSONServiceMutex);
  //подсчёт размера
  stLen = 0L;
  pfscList = pfscFirst;
  while(pfscList != NULL)
  {
    if(pfscList->rocEvent == ResultOfCompare::IS_EMPTY || pfscList->pszChanges == NULL)
    {
      pfscList = pfscList->pfscNext;
      continue;
    }
    if(pfscList->pszChanges != NULL)
      stLen = stLen + strlen(pfscList->pszChanges) + 1; //+1 на запятую
    else
    {
      stLen++;
      //если это начальная инициализация
      if(stType == INIT_SERVICE && pfscList->rocEvent == ResultOfCompare::DIRECTORY_END)
      {
	std::cerr << "JSONService::GetJSON() error: wrong JSON format!!!" << std::endl;
      }
    }
    pfscList = pfscList->pfscNext;
  }
  stLen++; //для '\0' в конце

  //создаём возвращаемую строку
  pszRet = new char[stLen+1];
  memset(pszRet, 0, stLen+1);

  //формируем окончательный JSON запрос
  pfscList = pfscLast = pfscFirst;
  while(pfscList != NULL)
  {
    if(pfscList->pszChanges != NULL)
    {
      if( pfscList->rocEvent != ResultOfCompare::DIRECTORY_END &&
	  !(pfscLast->nType == IS_DIRECTORY && ( pfscLast->rocEvent == ResultOfCompare::IS_EQUAL || pfscLast->rocEvent == ResultOfCompare::INIT_PROJECT )) &&
	  pfscLast->rocEvent != ResultOfCompare::INIT_PROJECT &&
	  pfscLast->rocEvent != ResultOfCompare::START_FILE_LIST &&
	  !(pfscLast->pfscNext != NULL && pfscLast->pfscNext->rocEvent == ResultOfCompare::END_FILE_LIST) &&
	  !(pfscLast->rocEvent == ResultOfCompare::START_CONTENT && ((pfscLast->pfscNext != NULL)&&(pfscLast->pfscNext->rocEvent == ResultOfCompare::END_CONTENT))) && //убираем содержание пустых директорий
	  pfscList != pfscFirst )
	strncat(pszRet, ",", stLen);
      strncat(pszRet, pfscList->pszChanges, stLen);
    }
    pfscLast = pfscList;
    pfscList = pfscList->pfscNext;
  }
  pthread_mutex_unlock(&mJSONServiceMutex);
  return pszRet;
}

//добавить в список запись об изменении файла
void JSONService::AddChange(ServiceType in_stType, DirSnapshot::FileData * const in_pfdFile, DirSnapshot::FileData const * const in_pfdParent, ResultOfCompare in_rocEvent)
{
  FSChange *pfscList;

  if(in_pfdFile == NULL || in_pfdFile->pName == NULL)
    return;

  pthread_mutex_lock(&mJSONServiceMutex);
  if(pfscFirst == NULL)
  {
    if(in_stType == INIT_SERVICE)
      pfscFirst = new FSChange(in_stType, in_pfdFile, NULL, in_rocEvent, NULL);
    else
    {
      //подготавливаем квадратные скобки для списка файлов
      pfscFirst = new FSChange(in_stType, NULL, NULL, ResultOfCompare::START_FILE_LIST, NULL);
      new FSChange(in_stType, NULL, NULL, ResultOfCompare::END_FILE_LIST, pfscFirst);
      new FSChange(in_stType, in_pfdFile, in_pfdParent, in_rocEvent, pfscFirst);
    }
    pthread_mutex_unlock(&mJSONServiceMutex);
    return;
  }

  //ищем нужный элемент списка
  pfscList = pfscFirst;
  while(pfscList != NULL)
  {
    if(in_stType == INIT_SERVICE && pfscList->pfdFile != NULL && in_pfdParent != NULL)
    {
      if((pfscList->pfdFile->stData.st_ino) == (in_pfdParent->stData.st_ino))
	break;
    }
    else if(in_stType == CURRENT_SERVICE)
    {
      if(pfscList->pfscNext != NULL && pfscList->pfscNext->rocEvent == ResultOfCompare::END_FILE_LIST)
	break;
    }
    pfscList = pfscList->pfscNext;
  }
  //если нужная директория не найдена - выходим
  if(pfscList == NULL)
  {
    pthread_mutex_unlock(&mJSONServiceMutex);
    return;
  }

  //добавляем событие в список
  new FSChange(in_stType, in_pfdFile, in_pfdParent, in_rocEvent, pfscList);
  pthread_mutex_unlock(&mJSONServiceMutex);
}

unsigned long JSONService::GetSessionNumber(void)
{
  return ulSessionNumber;
}

ServiceType JSONService::GetType(void)
{
  return stType;
}

//вернуть следующий список
JSONService * const JSONService::GetNext(void)
{
  return pjsNext;
}

void JSONService::SetNext(JSONService * const in_pjsNext)
{
  pjsNext = in_pjsNext;
}

//вывести содержимое списка
void JSONService::PrintService(void)
{
  FSChange *pfscList;
  
  pfscList = pfscFirst;
  while(pfscList != NULL)
  {
    std::cerr << "Session: " << ulSessionNumber << ", pfdFile=" << (unsigned long)pfscList->pfdFile << ",rocEvent=" << static_cast<int>(pfscList->rocEvent) << ",itInode=" << (unsigned long)pfscList->itInode << ",ttTime=" << pfscList->ttTime << ",pszChanges=" << pfscList->pszChanges     << ",pfscNext=" << (unsigned long)pfscList->pfscNext << std::endl;
    pfscList = pfscList->pfscNext;
  }
}

/************************************FSChange***************************************/

FSChange::FSChange()
{
  pfdFile = NULL;
  rocEvent = ResultOfCompare::IS_EMPTY;
  nType = IS_NOTAFILE;
  itInode = 0;
  ttTime = 0;
  pszChanges = NULL;

  pfscNext = NULL;
}

//перед добавлением записи необходимо найти место, куда её вставить
FSChange::FSChange(ServiceType in_stType, DirSnapshot::FileData * const in_pfdFile, DirSnapshot::FileData const * const in_pfdParent, ResultOfCompare in_rocEvent, FSChange * const in_pfscPrev)
{
  char szBuff[2048]; //буфер под запись
  char szType[32], szEvent[32], szCrc[32], szParent[32];
  size_t stLen;

  if(in_rocEvent == ResultOfCompare::IS_DELETED || in_rocEvent == ResultOfCompare::DIRECTORY_END || in_rocEvent == ResultOfCompare::START_FILE_LIST || in_rocEvent == ResultOfCompare::END_FILE_LIST)
    pfdFile = NULL;
  else
    pfdFile = in_pfdFile;

  if(in_pfdFile == NULL || in_pfdFile->pName == NULL)
  {
    if(in_rocEvent == ResultOfCompare::DIRECTORY_END || in_rocEvent == ResultOfCompare::START_FILE_LIST || in_rocEvent == ResultOfCompare::END_FILE_LIST)
      rocEvent = in_rocEvent;
    else
      rocEvent = ResultOfCompare::IS_EMPTY;
    nType = IS_NOTAFILE;
    itInode = 0;
    ttTime = 0;
    pszChanges = NULL;

    pfscNext = NULL;
  }
  else
  {
    rocEvent = in_rocEvent;
    nType = in_pfdFile->nType;
    itInode = in_pfdFile->stData.st_ino;
    ttTime = time(NULL);
  }

  //непосредственно создание записи в формате JSON
  memset(szBuff, 0, sizeof(szBuff));
  memset(szType, 0, sizeof(szType));
  memset(szCrc, 0, sizeof(szCrc));
  memset(szParent, 0, sizeof(szParent));
  if(in_pfdFile != NULL)
  {
    switch(in_pfdFile->nType)
    {
      case IS_FILE:
	strncpy(szType, "file", sizeof(szType));
	if(in_rocEvent != ResultOfCompare::IS_DELETED)
	  snprintf(szCrc, sizeof(szCrc), ",\"crc\":\"%ld\"", in_pfdFile->ulCrc);
	break;
      case IS_DIRECTORY:
	strncpy(szType, "folder", sizeof(szType));
	break;
      case IS_LINK:
	strncpy(szType, "link", sizeof(szType));
	if(in_rocEvent != ResultOfCompare::IS_DELETED)
	  snprintf(szCrc, sizeof(szCrc), ",\"crc\":\"%ld\"", in_pfdFile->ulCrc);
	break;
      default:
	strncpy(szType, "unknown", sizeof(szType));
    }
  }

  switch(in_rocEvent)
  {
    case ResultOfCompare::IS_EQUAL:
      strncpy(szEvent, "IS_EQUAL", sizeof(szEvent));
      break;
    case ResultOfCompare::IS_CREATED:
      strncpy(szEvent, "IS_CREATED", sizeof(szEvent));
      if(in_pfdParent != NULL)
        snprintf(szParent, sizeof(szParent), ",\"parent\":\"%ld\"", in_pfdParent->stData.st_ino);
      break;
    case ResultOfCompare::IS_DELETED:
      strncpy(szEvent, "IS_DELETED", sizeof(szEvent));
      if(in_pfdParent != NULL)
        snprintf(szParent, sizeof(szParent), ",\"parent\":\"%ld\"", in_pfdParent->stData.st_ino);
      break;
    case ResultOfCompare::NEW_NAME:
      strncpy(szEvent, "NEW_NAME", sizeof(szEvent));
      break;
    case ResultOfCompare::NEW_TIME:
      strncpy(szEvent, "NEW_TIME", sizeof(szEvent));
      break;
    case ResultOfCompare::NEW_HASH:
      strncpy(szEvent, "NEW_HASH", sizeof(szEvent));
      break;
    case ResultOfCompare::INIT_PROJECT:
      strncpy(szEvent, "INIT_PROJECT", sizeof(szEvent));
      break;
    default:
      strncpy(szEvent, "IS_EQUAL", sizeof(szEvent));
  }

  switch(in_rocEvent)
  {
    case ResultOfCompare::START_FILE_LIST:
      strncpy(szBuff, "[", sizeof(szBuff));
      break;
    case ResultOfCompare::END_FILE_LIST:
      strncpy(szBuff, "]", sizeof(szBuff));
      break;
    default:
      snprintf(szBuff, sizeof(szBuff)-1, "{\"type\":\"%s\",\"event\":\"%s\",\"name\":\"%s\",\"inode\":\"%ld\",\"time\":\"%ld\"%s%s%s",
					szType,
					szEvent,
					in_pfdFile->pName,
					itInode,
					ttTime,
					szCrc,
					szParent,
					(in_pfdFile->nType==IS_DIRECTORY && in_stType == INIT_SERVICE)?",\"content\":[":"}");
  }
  stLen = strlen(szBuff);
  pszChanges = new char[stLen + 1];
  memset(pszChanges, 0, stLen + 1);
  //формируем начало записи
  strncpy(pszChanges, szBuff, stLen);

  if(in_stType == INIT_SERVICE && in_pfdFile != NULL && in_pfdFile->nType == IS_DIRECTORY)
  {
    //исключаем рекурсию
    pfscNext = new FSChange();
    if(in_pfscPrev != NULL)
    {
      pfscNext->pfscNext = in_pfscPrev->pfscNext;
      in_pfscPrev->pfscNext = this;
    }
    pfscNext->rocEvent = ResultOfCompare::DIRECTORY_END;
    memset(szBuff, 0, sizeof(szBuff));
    strncpy(szBuff, "]}", sizeof(szBuff)-1);
    pfscNext->pszChanges = new char[stLen + 1];
    memset(pfscNext->pszChanges, 0, stLen);
    //формируем конец записи о директории
    strncpy(pfscNext->pszChanges, szBuff, stLen);
  }
  else
  {
    if(in_pfscPrev != NULL)
    {
      pfscNext = in_pfscPrev->pfscNext;
      in_pfscPrev->pfscNext = this;
    }
    else
      pfscNext = NULL;
  }
//   std::cerr << "FSChange::FSChange() : " << pszChanges << std::endl; //отладка!!!
}

FSChange::~FSChange()
{
  pfdFile = NULL;
  rocEvent = ResultOfCompare::IS_EMPTY;
  nType = IS_NOTAFILE;
  itInode = 0;
  ttTime = 0;
  if(pszChanges != NULL)
    delete [] pszChanges;
  pfscNext = NULL;
}
