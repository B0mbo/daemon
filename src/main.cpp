//File Status by Bombo
//10.01.2017

//потоки:
//1 - поток обработки ОЧЕРЕДИ дескрипторов, созданной из обработчика сигнала, стартует с этим приложением
//2 - потоки добавления дескрипторов в очередь на обработку, запускаемые из обработчика сигнала
//3 - поток обработки СПИСКА открытых директорий (дескрипторов), стартует вместе с этим приложением

#include<fcntl.h>
#include<cstring>
#include<unistd.h>
#include<iostream>
#include<signal.h>
#include<dirent.h> //opendir(), readdir()
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/select.h>
#include<sys/resource.h>

#include"RootMonitor.h"

#define THREAD_SLEEP_TIME 0

using namespace undefinedspace;

RootMonitor *rmProject;

void StartSomeThread(unsigned long in_ulStack, void *(* in_FunctionName)(void *), void *in_pvArg);
void sig_handler(int nsig, siginfo_t *siginfo, void *context);
void *fd_queue_thread(void *arg);
void *file_thread(void *arg);
void *directory_thread(void *arg);
void *send_json_thread(void *arg);

int main(int argc, char *argv[])
{
  int i;
  char filename[256];
  struct stat st;
  sigset_t set;
  struct sigaction signal_data;
  rlimit r;
  JSONParser *jp = new JSONParser();

//  std::cerr << "DEBUG!!!" << std::endl;
  jp->CheckRequest();
//  return 0;

  //увеличиваем лимит открытых файлов для текущего процесса
  r.rlim_cur = 8192;
  r.rlim_max = 8192;
  if(setrlimit(RLIMIT_NOFILE, &r) < 0)
    perror("main(), setrlimit error:");
  getrlimit(RLIMIT_NOFILE, &r);
  std::cerr << "main() : nofile limits: rlim_cur=" << r.rlim_cur << ", rlim_max=" << r.rlim_max << std::endl;

  pthread_mutex_unlock(&(RootMonitor::mDescListMutex));
  pthread_mutex_unlock(&(RootMonitor::mDescQueueMutex));
  pthread_mutex_lock(&(RootMonitor::mSendJSONThreadMutex));

  //проверяем количество аргументов
  if(argc <= 2)
  {
    std::cerr << "USAGE: " << argv[0] << " <path to directory 1> ... <path to directory N> <server URL[:Port]>" << std::endl;
    return -1;
  }

  //обнуляем описание сигнала
  memset(&signal_data, 0, sizeof(signal_data));
  //назначаем обработчик сигнала
  signal_data.sa_sigaction = &sig_handler;
  //сопровождаем сигнал дополнительной информацией
  signal_data.sa_flags = SA_SIGINFO;
  //обнуляем маску блокируемых сигналов
  sigemptyset(&set);
  //добавляем блокировку обработки SIGUSR1
  //sigaddset(&set, SIGUSR1);
  //инициализируем маску
  signal_data.sa_mask = set;
  if(sigaction(SIGUSR1, &signal_data, NULL) < 0)
    std::cerr << "sigaction(): can not activate signal" << std::endl;

  for(i = 0; i < argc-2; ++i)
  {
    //обнуляем имя файла
    memset(filename, 0, sizeof(filename));
    //копируем имя файла
    strncpy(filename, argv[i+1], sizeof(filename));
    if(filename[0] != '/')
    {
      std::cerr << "main() wrong path: A path to project directory must be full! (\"" << filename << "\")" << std::endl;
      continue;
    }

    //пытаемся открыть файл
    rmProject = new RootMonitor(filename);
    stat(filename, &st);
    std::cerr << "Project path: \"" << filename << "\", inode=" << st.st_ino << ", mode=" << (st.st_mode & S_IFDIR) << ", DIR=" <<  S_IFDIR << std::endl;
    break;
  }

  std::cerr << "Server URL: " << argv[argc-1] << std::endl;
  rmProject->SetServerURL(argv[argc-1]);

  //запускаем поток обработки сигнала
  StartSomeThread(102400L, file_thread, NULL);

  //запускаем поток обработки списка найденных директорий
  StartSomeThread(102400L, directory_thread, NULL);

  //запускаем поток отправки JSON-сообщений серверу
  StartSomeThread(102400L, send_json_thread, NULL);

  usleep(2000000); //отладка!!!
  char *list; //отладка!!!
  list = rmProject->GetJSON(rmProject->GetLastSessionNumber()); //отладка!!!
  std::cerr << ((list==NULL)?"NULL":list) << std::endl; //отладка!!!
  if(list != NULL) //отладка!!!
    delete [] list; //отладка!!!

  rmProject->SendChangesToServer(); //отладка!!!

  for(;;)
  {
    usleep(70000000);
  }

  return 0;
}

void StartSomeThread(unsigned long in_ulStack, void *(* in_FunctionName)(void *), void *in_pvArg)
{
  pthread_attr_t paAttr;
  pthread_t ptThread;

  pthread_attr_init(&paAttr);
  pthread_attr_setstacksize(&paAttr, in_ulStack);
  pthread_attr_setdetachstate(&paAttr, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptThread, &paAttr, in_FunctionName, in_pvArg);
  pthread_attr_destroy(&paAttr);
}

//обработчик сигнала об изменении в некотором файле
void sig_handler(int nsig, siginfo_t *siginfo, void *context)
{
  int *pFd;

  switch(nsig)
  {
    case SIGUSR1:
      {
        //готовим параметр для передачи в поток
        pFd = new int(siginfo->si_fd); //выделяем память для параметра
        //создаём поток постановки дескриптора на обработку
        StartSomeThread(102400L, fd_queue_thread, (void *)pFd);
      }
      break;
  }
}

//поток постановки дескриптора в очередь на обработку
//поскольку таких потоков может породиться много за короткое время,
//сама обработка происходит в другом потоке
void *fd_queue_thread(void *arg)
{
  int nFd;

  nFd = *((int *) arg);

  delete (int *)arg; //освобождаем ранее выделенную для параметра память

  if(RootMonitor::pdqQueue != NULL)
    RootMonitor::pdqQueue->AddDescriptor(nFd);
  else
  {
    std::cerr << "очередь ещё не инициализирована!" << std::endl;
    pthread_exit(NULL);
  }

  //сообщаем о новом сигнале обработчику
  pthread_mutex_unlock(&(RootMonitor::mDescThreadMutex));
  pthread_exit(NULL);
}

//поток обработки очереди дескрипторов
void *file_thread(void *arg)
{
  int nFd;
  SomeDirectory *psdDir;

  for(;;)
  {
    //чуть притормозим
    if(THREAD_SLEEP_TIME)
      usleep(THREAD_SLEEP_TIME);
    pthread_mutex_lock(&(RootMonitor::mDescThreadMutex));
    pthread_mutex_lock(&(RootMonitor::mDescQueueMutex)); //очередь обрабатываемых дескрипторов
    //удаление дескриптора из очереди дескрипторов
    if(RootMonitor::pdqQueue != NULL)
    {
      nFd = RootMonitor::pdqQueue->GetDescriptor();
      while(nFd != -1)
      {
        //сравнение слепков директорий
        psdDir = RootMonitor::pdlList->GetDirectory(nFd);
        if(psdDir != NULL)
          psdDir->CompareSnapshots();
        else
          std::cerr << "fd_queue_thread() : No such directory!" << std::endl;

        if(nFd >= 0)
        {
          //после обработки сигнала обработчик сбрасывается на тот, что был по умолчанию
          //поэтому здесь обновляем обработчик сигнала для дескриптора
          //вешаем сигнал на дескриптор
          //возможно, имеет смысл убрать это обновление в метод класса DecriptorsQueue или DescriptorsList
          if(fcntl(nFd, F_SETSIG, SIGUSR1) < 0)
          {
            perror("fcntl");
            close(nFd);
            std::cerr << "Can not init signal for fd" << std::endl;
            continue;
          }
          //устанавливаем типы оповещений
          if(fcntl(nFd, F_NOTIFY, DN_MODIFY|DN_CREATE|DN_DELETE|DN_RENAME/*|DN_ACCESS*/) < 0)
          {
            perror("fcntl");
            close(nFd);
            std::cerr << "Can not set types for the signal" << std::endl;
            continue;
          }
        }
        nFd = RootMonitor::pdqQueue->GetDescriptor();
      }
    }
    pthread_mutex_unlock(&(RootMonitor::mDescQueueMutex)); //освобождение очереди дескрипторов
  }
  pthread_exit(NULL);
}

//поток обработки списка найденных директорий
void *directory_thread(void *arg)
{
  for(;;)
  {
    pthread_mutex_lock(&(RootMonitor::mDirThreadMutex));
    //обработка списка директорий
    RootMonitor::pdlList->UpdateList();
    //  	RootMonitor::pdlList->PrintList();
    if(THREAD_SLEEP_TIME)
      usleep(THREAD_SLEEP_TIME);
  }
  pthread_exit(NULL);
}

void *send_json_thread(void *)
{
  for(;;)
  {
    pthread_mutex_lock(&(RootMonitor::mSendJSONThreadMutex));
    if(rmProject != NULL)
      rmProject->SendChangesToServer();
  }
  pthread_exit(NULL);
}
