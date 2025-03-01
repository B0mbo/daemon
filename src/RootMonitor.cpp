//RootMonitor.cpp
//Author: Bombo
//12.01.2017
//Класс RootMonitor отвечает за один отдельный проект для наблюдения

#include"RootMonitor.h"

using namespace undefinedspace;

DescriptorsList *RootMonitor::pdlList = NULL;
DescriptorsQueue *RootMonitor::pdqQueue = NULL;
pthread_mutex_t RootMonitor::mDescListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t RootMonitor::mDescQueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t RootMonitor::mDirThreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t RootMonitor::mDescThreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t RootMonitor::mSendJSONThreadMutex = PTHREAD_MUTEX_INITIALIZER;

RootMonitor::RootMonitor()
{
    psdRootDirectory = NULL;
    pdlList = NULL;
    pjsFirst = NULL;
    ulLastSessionNumber = 0L;
    ulRegularSessionNumber = 4096L;
    pszServerURL = NULL;
    aiRes = NULL;

    mSocketMutex = PTHREAD_MUTEX_INITIALIZER;
    mEventsQueueMutex = PTHREAD_MUTEX_INITIALIZER;
}

RootMonitor::RootMonitor(char * const pRootPath)
{
    pjsFirst = NULL;
    ulLastSessionNumber = 0L;
    ulRegularSessionNumber = 4096L;
    pszServerURL = NULL;
    aiRes = NULL;
    mSocketMutex = PTHREAD_MUTEX_INITIALIZER;
    mEventsQueueMutex = PTHREAD_MUTEX_INITIALIZER;

    if(pRootPath == NULL)
    {
	psdRootDirectory = NULL;
	pdlList = NULL;
	return;
    }

    if(pdqQueue == NULL)
	pdqQueue = new DescriptorsQueue();

    //создаём описание корневой директории
    psdRootDirectory = new SomeDirectory(pRootPath, NULL);

    //создаём первый список событий (инициализирующий)
    AddChange(INIT_SERVICE, ulLastSessionNumber, psdRootDirectory->GetFileData(), NULL, ResultOfCompare::INIT_PROJECT);

    //открываем корневую директорию и добавляем полученный дескриптор в список открытых
    //этот список существует для упрощения поиска директории по её дескриптору
    if(pdlList == NULL)
    {
	pthread_mutex_lock(&mDescListMutex);
	pdlList = new DescriptorsList(psdRootDirectory);
	pthread_mutex_unlock(&mDescListMutex);
    }
    else
    {
	pthread_mutex_lock(&mDescListMutex);
	pdlList->AddQueueElement(psdRootDirectory);
	pthread_mutex_unlock(&mDescListMutex);
    }
}

RootMonitor::RootMonitor(DirSnapshot::FileData * const in_pfdData)
{
    pjsFirst = NULL;
    ulLastSessionNumber = 0L;
    ulRegularSessionNumber = 4096L;
    pszServerURL = NULL;
    aiRes = NULL;
    mSocketMutex = PTHREAD_MUTEX_INITIALIZER;
    mEventsQueueMutex = PTHREAD_MUTEX_INITIALIZER;

    if(in_pfdData == NULL)
    {
	psdRootDirectory = NULL;
	pdlList = NULL;
	return;
    }

    if(pdqQueue == NULL)
	pdqQueue = new DescriptorsQueue();

    //создаём описание корневой директории
    psdRootDirectory = new SomeDirectory(in_pfdData, NULL, true);

    //создаём первый список событий (инициализирующий)
    AddChange(INIT_SERVICE, ulLastSessionNumber, psdRootDirectory->GetFileData(), NULL, ResultOfCompare::INIT_PROJECT);

    //открываем корневую директорию и добавляем полученный дескриптор в список открытых
    if(pdlList == NULL)
    {
	pthread_mutex_lock(&mDescListMutex);
	pdlList = new DescriptorsList(psdRootDirectory);
	pthread_mutex_unlock(&mDescListMutex);
    }
    else
    {
	pthread_mutex_lock(&mDescListMutex);
	pdlList->AddQueueElement(psdRootDirectory);
	pthread_mutex_unlock(&mDescListMutex);
    }
}

RootMonitor::RootMonitor(SomeDirectory * const in_psdRootDirectory)
{
    pjsFirst = NULL;
    ulLastSessionNumber = 0L;
    ulRegularSessionNumber = 4096L;
    pszServerURL = NULL;
    aiRes = NULL;
    mSocketMutex = PTHREAD_MUTEX_INITIALIZER;
    mEventsQueueMutex = PTHREAD_MUTEX_INITIALIZER;

    if(in_psdRootDirectory == NULL)
    {
	psdRootDirectory = NULL;
	pdlList = NULL;
	return;
    }

    if(pdqQueue == NULL)
	pdqQueue = new DescriptorsQueue();

    //инициализируем ссылку на описание корневой директории отслеживаемого проекта
    psdRootDirectory = in_psdRootDirectory;

    //создаём первый список событий (инициализирующий)
    AddChange(INIT_SERVICE, ulLastSessionNumber, psdRootDirectory->GetFileData(), NULL, ResultOfCompare::INIT_PROJECT);

    //открываем корневую директорию и добавляем полученный дескриптор в список открытых
    if(pdlList == NULL)
    {
	pthread_mutex_lock(&mDescListMutex);
	pdlList = new DescriptorsList(in_psdRootDirectory);
	pthread_mutex_unlock(&mDescListMutex);
    }
    else
    {
	pthread_mutex_lock(&mDescListMutex);
	pdlList->AddQueueElement(in_psdRootDirectory);
	pthread_mutex_unlock(&mDescListMutex);
    }
}

RootMonitor::~RootMonitor()
{
    //т.к. эти данные теперь статические, их не следует удалять
//    if(pdlList != NULL)
//        delete pdlList;
//    if(psdRootDirectory != NULL)
//        delete psdRootDirectory;
    DeleteJSONServices();

    pthread_mutex_lock(&mSocketMutex);
    if(pszServerURL != NULL)
      delete [] pszServerURL;
    if(aiRes != NULL)
    {
      freeaddrinfo(aiRes);
      aiRes = NULL;
    }
    pthread_mutex_unlock(&mSocketMutex);
    pthread_mutex_destroy(&mSocketMutex);
    pthread_mutex_destroy(&mEventsQueueMutex);
}

void RootMonitor::AddChange(ServiceType in_stType, unsigned long in_ulSessionNumber, DirSnapshot::FileData * const in_pfdFile, DirSnapshot::FileData const * const in_pfdParent, ResultOfCompare in_rocEvent)
{
  JSONService *pjsList, *pjsLast, *pjsBuff;

  pthread_mutex_lock(&mEventsQueueMutex);

  if(pjsFirst == NULL)
    pjsFirst = new JSONService(in_stType, in_ulSessionNumber);

  pjsList = pjsLast = pjsFirst;
  while(pjsList != NULL)
  {
    pjsLast = pjsList;
    if( ((pjsLast->GetSessionNumber()) == in_ulSessionNumber) && ((pjsLast->GetType()) == in_stType) )
      break;
    pjsList = pjsList->GetNext();
  }

  //pjsLast указывает либо на найденный сервис, либо на последний сервис в списке
  if(pjsList == NULL)
  {
    pjsBuff = pjsLast->GetNext();
    pjsList = new JSONService(in_stType, in_ulSessionNumber);
    pjsLast->SetNext(pjsList);
    pjsList->SetNext(pjsBuff);
  }

  pjsList->AddChange(in_stType, in_pfdFile, in_pfdParent, in_rocEvent);

  pthread_mutex_unlock(&mEventsQueueMutex);
}

//добавить в очередь инициализирующее событие для данного проекта
void RootMonitor::AddInitChange(DirSnapshot::FileData * const in_pfdFile, DirSnapshot::FileData const * const in_pfdParent)
{
  AddChange(INIT_SERVICE, ulLastSessionNumber, in_pfdFile, in_pfdParent, ResultOfCompare::IS_EQUAL);
}

//получить JSON конкретной сессии
char * const RootMonitor::GetJSON(unsigned long in_ulSessionNumber)
{
  JSONService *pjsList;

  if(pjsFirst == NULL)
    return NULL;

  pthread_mutex_lock(&mEventsQueueMutex);
  pjsList = pjsFirst;
  while(pjsList != NULL)
  {
    if((pjsList->GetSessionNumber()) == in_ulSessionNumber)
    {
      pthread_mutex_unlock(&mEventsQueueMutex);
      return (pjsList->GetJSON());
    }
    pjsList = pjsList->GetNext();
  }
  pthread_mutex_unlock(&mEventsQueueMutex);
  return NULL;
}

//инициализация адреса сервера и получение списка ip
void RootMonitor::SetServerURL(char const * const in_pszServerURL)
{
  struct addrinfo *aiPreRes, *aiList;
  struct addrinfo aiMask;
  char *pszAddr, *pPort;
  int nRes, nIndex;
  int sPreSocket;
  char szPort[8];
  size_t stLen;

  if(in_pszServerURL == NULL)
    return;

  stLen = strlen(in_pszServerURL);
  pszAddr = new char[stLen+1];
  memset(pszAddr, 0, stLen + 1);
  strncpy(pszAddr, in_pszServerURL, stLen);
  nIndex = 0;
  while(pszAddr[nIndex] != ':' && nIndex < stLen)
  {
    nIndex++;
  }
  //если порт задан
  pPort = NULL;
  if(nIndex < stLen)
  {
    pPort = pszAddr + nIndex + 1;
    pszAddr[nIndex] = '\0';
  }
  snprintf(szPort, sizeof(szPort), "%s", pPort?pPort:"9999");

  pthread_mutex_lock(&mSocketMutex);

  memset(&aiMask, 0, sizeof(addrinfo));
  aiMask.ai_family = AF_INET;
  aiMask.ai_socktype = SOCK_STREAM;
  if(getaddrinfo(pszAddr, szPort, &aiMask, &aiPreRes) != 0)
  {
    perror("RootMonitor::SetServerURL() getaddrinfo error");
    pthread_mutex_unlock(&mSocketMutex);
    return;
  }
  //проверяем работоспособность найденных адресов
  for(aiList = aiPreRes; aiList != NULL; aiList = aiList->ai_next)
  {
    sPreSocket = socket(aiList->ai_family, aiList->ai_socktype, aiList->ai_protocol);
    if(sPreSocket == -1)
      continue;
    if((nRes = connect(sPreSocket, aiList->ai_addr, aiList->ai_addrlen)) == 0)
    {
      close(sPreSocket);
      break;
    }
    else
    {
      perror("RootMonitor::SetServerURL() connect error");
    }
    close(sPreSocket);
  }

  //если адреса не рабочие
  if(aiList == NULL)
  {
    pthread_mutex_unlock(&mSocketMutex);
    return;
  }

  if(aiRes != NULL)
    freeaddrinfo(aiRes);

  aiRes = aiPreRes;

  if(pszServerURL != NULL)
  {
    delete [] pszServerURL;
  }
  stLen = strlen(in_pszServerURL) + 1;
  pszServerURL = new char[stLen];
  memset(pszServerURL, 0, stLen);
  strncpy(pszServerURL, in_pszServerURL, stLen-1);

  pthread_mutex_unlock(&mSocketMutex);
}

//получить адрес сервера
char const * const RootMonitor::GetServerURL(void)
{
  return pszServerURL;
}

int RootMonitor::SendData(char * const in_pData, size_t in_stLen, bool in_fDeleteString)
{
  struct addrinfo *aiList;
  size_t stSent, stVolume;
  int sSocket;

  if(aiRes == NULL || pszServerURL == NULL || in_pData == NULL)
    return -1;

  pthread_mutex_lock(&mSocketMutex);
  for(aiList = aiRes; aiList != NULL; aiList = aiList->ai_next)
  {
    sSocket = socket(aiList->ai_family, aiList->ai_socktype, aiList->ai_protocol);
    if(sSocket == -1)
      continue;
    if(connect(sSocket, aiList->ai_addr, aiList->ai_addrlen) < 0)
    {
      close(sSocket);
      continue;
    }
    stVolume = 0;
    do {
      stSent = send(sSocket, in_pData, in_stLen, 0);
      //заменить паузу на блокировку дескриптора через fcntl (!)
      //...
      usleep(1000000);
      if(stSent < 0)
      {
	close(sSocket);
	continue;
      }
      stVolume = stVolume + stSent;
    }
    while(stVolume < in_stLen);
    usleep(500000);
    close(sSocket);
    break;
  }

  if(in_fDeleteString)
    delete [] in_pData;

  pthread_mutex_unlock(&mSocketMutex);

  return stVolume;
}

void RootMonitor::SendChangesToServer(void)
{
  size_t stLen;
  int nTypeNumber;
  char *pszBuff, *pszJSON;
  JSONService *pjsList, *pjsLast;
  ServiceType stTypes[] = {INIT_SERVICE, CURRENT_SERVICE, NO_SERVICE};
  char szRequest[] = "\
%s /api/v1/sync HTTP/1.1\r\n\
Host: %s\r\n\
Content-Length: %d\r\n\
Content-Type: application/json\r\n\
Connection: keep-alive\
\r\n\
\r\n\
%s";

  if(pjsFirst == NULL || pszServerURL == NULL)
    return;

  pthread_mutex_lock(&mEventsQueueMutex);

  nTypeNumber = 0;
  while(stTypes[nTypeNumber] != NO_SERVICE)
  {
    //отправляем инициализацию
    pjsList = pjsLast = pjsFirst;
    while(pjsList != NULL)
    {
      if((pjsList->GetType()) == stTypes[nTypeNumber])
      {
	//формируем сообщение для отправки
	pszJSON = pjsList->GetJSON();
	if(pszJSON != NULL)
	{
	  stLen = strlen(szRequest) + strlen(pszJSON) + strlen(pszServerURL) + 16;
	  pszBuff = new char[stLen];
	  memset(pszBuff, 0, stLen);
	  snprintf(pszBuff, stLen-1, szRequest, (stTypes[nTypeNumber] == INIT_SERVICE)?"POST":"PUT",pszServerURL, strlen(pszJSON), pszJSON);
//	  std::cerr << "\n%s\n", pszBuff); //отладка!!!
	  delete [] pszJSON;

	  //отправляем изменения (строка сама удалится после отправки)
	  SendData(pszBuff, strlen(pszBuff), true);
	  //удаляем отправленный сервис
	  if(pjsList == pjsFirst)
	    pjsFirst = pjsList->GetNext();
	  else
	    pjsLast->SetNext(pjsList->GetNext());
	  delete pjsList;
	  pjsList = pjsLast;
	}
      }
      pjsLast = pjsList;
      pjsList = pjsList->GetNext();
    }
    nTypeNumber++;
  }

  pthread_mutex_unlock(&mEventsQueueMutex);
}

unsigned long RootMonitor::GetLastSessionNumber(void)
{
  return ulLastSessionNumber;
}

unsigned long RootMonitor::GetRegularSessionNumber(void)
{
  return ulRegularSessionNumber;
}

void RootMonitor::IncRegularSessionNumber(void)
{
  pthread_mutex_lock(&mEventsQueueMutex);
  ulRegularSessionNumber++;
  pthread_mutex_unlock(&mEventsQueueMutex);
}

//поменять/установить путь к корневой директории
int RootMonitor::SetRootPath(char const * const in_pNewRootPath)
{
    //останавливаем сопровождающие потоки
    //...

//    SetDirName(in_pNewRootPath); //функцию необходимо переделать, поэтому закомментировано

    //запускаем потоки
    //...

/*
    size_t stLen;
    int nNewRootFd;

    //если путь указан неверно
    if(in_pNewRootPath == NULL || (stLen = strlen(in_pNewRootPath)) <= 0)
    {
	return -1;
    }

    if((nNewRootFd = open(in_pNewRootPath, O_RDONLY)) < 0)
    {
	return -2;
    }

    //закрываем имеющийся дескриптор (если он открыт)
    if(nRootFd >= 0)
        close(nRootFd);
    //удаляем прежний путь
    if(pRootPath != NULL)
	delete [] pRootPath;
    //удаляем копию пути
    if(pSafeRootPath != NULL)
	delete [] pSafeRootPath;

    //задаём дескриптор
    nRootFd = nNewRootFd;
    //создаём новый путь
    pRootPath = new char[stLen+1];
    memset(pRootPath, 0, stLen+1);
    strncpy(pRootPath, in_pNewRootPath, stLen);
    //копируем путь
    pSafeRootPath = new char[stLen+1];
    memset(pSafeRootPath, 0, stLen+1);
    strncpy(pSafeRootPath, in_pNewRootPath, stLen);
*/
    return 0;
}

void RootMonitor::DeleteJSONServices(void)
{
    JSONService *pjsList, *pjsDel;

    pthread_mutex_lock(&mEventsQueueMutex);
    pjsList = pjsDel = pjsFirst;
    while(pjsList != NULL)
    {
	pjsList = pjsList->GetNext();
	delete pjsDel;
	pjsDel = pjsList;
    }
    pjsFirst = NULL;
    pthread_mutex_unlock(&mEventsQueueMutex);
}

//вывести содержимое списка с конкретным номером сессии
void RootMonitor::PrintSession(unsigned long in_ulSessionNumber)
{
    JSONService *pjsList;
    
    pjsList = pjsFirst;
    while(pjsList != NULL)
    {
	if((pjsList->GetSessionNumber()) == in_ulSessionNumber)
	{
	    pjsList->PrintService();
	    return;
	} 
	pjsList = pjsList->GetNext();
    }
}

//вывести содержимое списка с конкретным номером сессии
void RootMonitor::PrintServices(void)
{
    JSONService *pjsList;
    
    pjsList = pjsFirst;
    while(pjsList != NULL)
    {
	pjsList->PrintService();
	pjsList = pjsList->GetNext();
    }
}
