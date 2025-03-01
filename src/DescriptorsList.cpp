//DescriptorsList.cpp
//Author: Bombo
//13.01.2017
//Класс DescriptorsList содержит список всех открытых дескрипторов

#include"DescriptorsList.h"

using namespace undefinedspace;

/*********************************DescriptorQueue*********************************/

DescriptorsList::DescriptorsList()
{
    //инициализация блокировки списка директорий
    mListMutex = PTHREAD_MUTEX_INITIALIZER;
    //пустой первый элемент
    pthread_mutex_lock(&mListMutex);
    pdleFirst = NULL;
    pthread_mutex_unlock(&mListMutex);
}

DescriptorsList::DescriptorsList(SomeDirectory *in_psdRootDirectory)
{
    //инициализация блокировки списка директорий
    mListMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_unlock(&mListMutex);
    //создаём список директорий с первым элементом
    pthread_mutex_lock(&mListMutex);
    pdleFirst = new DirListElement(in_psdRootDirectory, NULL);
    pthread_mutex_unlock(&mListMutex);
}

DescriptorsList::DescriptorsList(DirSnapshot::FileData *in_pfdData)
{
    //инициализация блокировки списка директорий
    mListMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_unlock(&mListMutex);
    //создаём список директорий с первым элементом
    pthread_mutex_lock(&mListMutex);
    pdleFirst = new DirListElement(in_pfdData, NULL, NULL);
    pthread_mutex_unlock(&mListMutex);
}

DescriptorsList::~DescriptorsList()
{
    //удаляем ссписок директорий
    DirListElement *pdleList, *pdleDel;
    if(pdleFirst == NULL)
	return;
    pthread_mutex_lock(&mListMutex);
    pdleList = pdleFirst->pdleNext;
    while(pdleList != NULL)
    {
	pdleDel = pdleList;
	pdleList = pdleList->pdleNext;
	delete pdleDel;
    }
    delete pdleFirst;
    pdleFirst = NULL;
    pthread_mutex_unlock(&mListMutex);
}

//перед вызовом метода ОБЯЗАТЕЛЬНО следует блокировать mDescListMutex
//после вызова - освобождать
void DescriptorsList::AddQueueElement(SomeDirectory * const in_psdPtr)
{
    DirListElement *pdleList;

    if(in_psdPtr == NULL)
	return;

    //добавляем элемент списка
    if(pdleFirst == NULL)
    {
	pthread_mutex_lock(&mListMutex);
	//если список пуст - назначаем первый элемент
	pdleFirst = new DirListElement(in_psdPtr, NULL);
	pthread_mutex_unlock(&mListMutex);
	return;
    }

    pthread_mutex_lock(&mListMutex);
    //ищем конец списка
    pdleList = pdleFirst;
    while(pdleList->pdleNext != NULL)
    {
	pdleList = pdleList->pdleNext;
    }

    //добавляем директорию в конец списка
    pdleList->pdleNext = new DirListElement(in_psdPtr, pdleList);
    pthread_mutex_unlock(&mListMutex);
}

void DescriptorsList::SubQueueElement(SomeDirectory const * const in_psdPtr)
{
    DirListElement *pdleList;

    //если список пуст - выходим
    if(pdleFirst == NULL)
	return;

    pthread_mutex_lock(&mListMutex);
    //ищем заданный элемент
    pdleList = pdleFirst;
    while(pdleList != NULL)
    {
	if(pdleList->psdDirectory == in_psdPtr)
	{
	    //удаляем найденный элемент из списка
	    if(pdleFirst == pdleList)
	      pdleFirst = pdleList->pdleNext;
	    delete pdleList;
	    pthread_mutex_unlock(&mListMutex);
	    return;
	}
    }
    pthread_mutex_unlock(&mListMutex);
}

void DescriptorsList::SubQueueElement(int in_nDirFd)
{
    int nDirFd;
    DirListElement *pdleList;

    //если список пуст - выходим
    if(pdleFirst == NULL)
	return;

    pthread_mutex_lock(&mListMutex);
    pdleList = pdleFirst;
    while(pdleList != NULL)
    {
	nDirFd = pdleList->psdDirectory->GetDirFd();
//	std::cerr << "DescriptorsList::SubQueueElement() : %d, %d\n", (int)nDirFd, (int)in_nDirFd); //отладка!!!
	if(nDirFd == in_nDirFd)
	{
	    //удаляем элемент из очереди
	    if(pdleFirst == pdleList)
	      pdleFirst = pdleList->pdleNext;
	    delete pdleList;
	    pthread_mutex_unlock(&mListMutex);
	    return;
	}
	pdleList = pdleList->pdleNext;
    }
    pthread_mutex_unlock(&mListMutex);
}

//переименовать директорию
void DescriptorsList::RenameQueueElement(DirSnapshot::FileData const * const in_pfdNewData)
{
    DirListElement *pdleList;

    if(pdleFirst == NULL)
	return;

    pthread_mutex_lock(&mListMutex);
    pdleList = pdleFirst;
    while(pdleList != NULL)
    {
	if(pdleList->psdDirectory->GetFileData()->stData.st_ino == in_pfdNewData->stData.st_ino)
	{
	    //переименовываем директорию
	    pdleList->psdDirectory->SetDirName(in_pfdNewData->pName);
	    pthread_mutex_unlock(&mListMutex);
	    return;
	}
	pdleList = pdleList->pdleNext;
    }
    pthread_mutex_unlock(&mListMutex);
}

//функция для отладки
int DescriptorsList::GetFd(void)
{
    static DirListElement *pdleLast;

    if(pdleLast == NULL)
    {
	pdleLast = pdleFirst;
	return pdleFirst->psdDirectory->GetDirFd();
    }

    pdleLast = pdleLast->pdleNext;

    if(pdleLast == NULL)
	return -1;

    return pdleLast->psdDirectory->GetDirFd();
}

//вывести список директорий
void DescriptorsList::PrintList(void)
{
    DirListElement *pdleList;
    DirSnapshot::FileData *pfdData;

    //если список пуст - выходим
    if(pdleFirst == NULL)
    {
	std::cerr << std::endl << "DescriptorsList::PrintList() : DirectoryList is empty!" << std::endl << std::endl;
	return;
    }

    std::cerr << std::endl << "DescriptorsList::PrintList() : DirectoryList: начало списка." << std::endl;
    pthread_mutex_lock(&mListMutex);
    pdleList = pdleFirst;
    while(pdleList != NULL)
    {
	pfdData = pdleList->psdDirectory->GetFileData();
	if(pfdData != NULL)
	  std::cerr << "DescriptorsList::PrintList() : DirectoryList: файл " << pdleList->psdDirectory->GetDirName() << ", fd=" << pdleList->psdDirectory->GetDirFd() << ", inode=" << (int)pfdData->stData.st_ino << "." <<  std::endl;
	pdleList = pdleList->pdleNext;
    }
    std::cerr << "DescriptorsList::PrintList() : DirectoryList: конец списка." << std::endl << std::endl;
    pthread_mutex_unlock(&mListMutex);
}

void DescriptorsList::UpdateList(void)
{
    DirListElement *pdleList;
    char *pPath = NULL;
    int nDirFd;
    DirSnapshot::FileData *pfdData;

//     std::cerr << "DescriptorsList::UpdateList() : start" << std::endl; //отладка!!!
    if(pdleFirst == NULL)
    {
	std::cerr << "DescriptorsList::UpdateList(): the list is empty!" << std::endl; //отладка!!!
	return;
    }
//    pthread_mutex_lock(&mListMutex); (?)
    //ищем новые директории и обновляем их
    pdleList = pdleFirst;
    while(pdleList != NULL)
    {
	if(pdleList->psdDirectory == NULL)
	{
	  std::cerr << "DescriptorsList::UpdateList() Update: NULL!" << std::endl; //отладка!!!
	  continue;
	}
	pfdData = pdleList->psdDirectory->GetFileData();
	//создаём слепки для новых директорий в списке
	if(pdleList->psdDirectory != NULL && pdleList->psdDirectory->IsSnapshotNeeded() && pfdData != NULL)
	{
	    pdleList->psdDirectory->MakeSnapshot(true);
	    //проверяем, открыта уже директория или ещё нет
	    nDirFd = pfdData->nDirFd;
	    if(nDirFd == -1) //отладка!!!
	    {
	      pPath = pdleList->psdDirectory->GetFullPath();
	      if(pPath != NULL)
	      {
		//учесть при инкапсуляции (!)
		pfdData->nDirFd = open(pPath, O_RDONLY);
		delete [] pPath;
	      }
	    }
	    nDirFd = pfdData->nDirFd;
	    if(nDirFd >= 0)
	    {
		//назначаем обработчик сигнала для дескриптора
		if(fcntl(nDirFd, F_SETSIG, SIGUSR1) != -1)
		{
		    if(fcntl(nDirFd, F_NOTIFY, DN_MODIFY|DN_CREATE|DN_DELETE|DN_RENAME/*|DN_ACCESS*/) == -1)
		    {
// 			struct stat st; //отладка!!!
// 			fstat(nDirFd, &st); //отладка!!!
// 			pPath = pdleList->psdDirectory->GetFullPath(); //отладка!!!
// 			std::cerr << "DescriptorsList::UpdateList() : \"%s\", fd=%d, st.st_ino=%d\n", pPath, nDirFd, (int)st.st_ino); //отладка!!!
// 			if(pPath != NULL) //отладка!!!
// 			  delete [] pPath; //отладка!!!
			perror("DescriptorsList::UpdateList() ???невозможно назначить обработку для дескриптора");
		    }

		    //инициализацию первого элемента нужно проводить в конструкторе RootMonitor
		    //добавление остальных файлов в конструкторе DirSnapshot (временно, т.к. там могут создаваться слепки в результате обработки сигналов)
		    //если создаётся невременный слепок - это создание инициализирующего списка файлов (т.е. вызывать для инициализации надо в конструкторе слепка)
		    //если происходит обработка сравнения слепков - это создание вторичного списка событий (т.е. вторичный список создаётся в обработчике сравнений слепков)
		}
		else
		{
		    perror("DescriptorsList::UpdateList(), fcntl");
		    close(nDirFd);
		    //учесть после инкапсуляции (!)
		    pfdData->nDirFd = -1;
		}
	    }
	}
	else
	{
// 	  if(pdleList->psdDirectory->IsSnapshotNeeded())
// 	    std::cerr << "DescriptorsList::UpdateList() 3: no name or file data!" << std::endl;
	}
	pdleList = pdleList->pdleNext;
    }
//    pthread_mutex_unlock(&mListMutex); (?)
}

SomeDirectory *DescriptorsList::GetDirectory(int in_nFd)
{
    DirListElement *pdleDir;

    if(pdleFirst == NULL)
	return NULL;

    pdleDir = pdleFirst;
    while(pdleDir != NULL)
    {
	if(pdleDir->psdDirectory->GetDirFd() == in_nFd)
	    return pdleDir->psdDirectory;
	pdleDir = pdleDir->pdleNext;
    }

    return NULL;
}

/*********************************DirListElement*********************************/

DescriptorsList::DirListElement::DirListElement()
{
    psdDirectory = NULL;
    pdleNext = NULL;
    pdlePrev = NULL;
}

DescriptorsList::DirListElement::DirListElement(SomeDirectory *in_psdDirectory, DirListElement * const in_pdlePrev)
{
    psdDirectory = in_psdDirectory;
    pdlePrev = in_pdlePrev;

//     std::cerr << "DirListElement::DirListElement() : inode=" << psdDirectory->GetDirSnapshot::FileData())->stData.st_ino << std::endl; //отладка!!!
    //исключаем выпадение части элементов списка
    if(in_pdlePrev != NULL)
    {
        pdleNext = in_pdlePrev->pdleNext;
	//если за предшествующим элементом есть последующий, меняем у последующего предыдущий на this
        if(in_pdlePrev->pdleNext != NULL)
	    in_pdlePrev->pdleNext->pdlePrev = this;
	in_pdlePrev->pdleNext = this;
    }
    else
    {
	pdleNext = NULL;
    }
}

DescriptorsList::DirListElement::DirListElement(DirSnapshot::FileData *in_pfdData, SomeDirectory * const in_psdParent, DirListElement * const in_pdlePrev)
{
    //осторожно! Родительский каталог не ищется! Надо делать! Краш!!!
    psdDirectory = new SomeDirectory(in_pfdData, in_psdParent, true); //снимок обязан присутствовать в элементе очереди
    pdlePrev = in_pdlePrev;
    //исключаем выпадение части элементов списка
    if(in_pdlePrev != NULL)
    {
        pdleNext = in_pdlePrev->pdleNext;
	//если за предшествующим элементом есть последующий, меняем у последующего предыдущий на this
        if(in_pdlePrev->pdleNext != NULL)
	    in_pdlePrev->pdleNext->pdlePrev = this;
	in_pdlePrev->pdleNext = this;
    }
    else
    {
	pdleNext = NULL;
    }
}

DescriptorsList::DirListElement::~DirListElement()
{
    //полностью удаляем директорию
    //DirSnapshot::FileData удаляется из своего слепка автоматически
    delete psdDirectory;
    //обновляем очередь
    if(pdlePrev != NULL)
      pdlePrev->pdleNext = pdleNext;
    if(pdleNext != NULL)
      pdleNext->pdlePrev = pdlePrev;
}
