//DescriptrorsQueue.h
//Author: Bombo
//18.01.2017
//Класс DescriptorsQueue сдержит очередь открытых дескрипторов директорий, в которых
//произошли некоторые изменения. Дескрипторы из этой очереди ожедают обрабтки в той
//же последовательности, в какой они были добавлены.

#pragma once

#include<iostream>
#include<pthread.h>

namespace undefinedspace {

class DescriptorsQueue
{
    struct DescriptorNum
    {
        int nFd;

        //указатель на следующий дескриптор в очереди
        DescriptorNum *pdnNext;

        DescriptorNum(int in_nFd);
        ~DescriptorNum(void);
    };

    //указатель на первый элемент в очереди
    DescriptorNum *pdnFirst;
    pthread_mutex_t mFdQueueMutex;

public:
    DescriptorsQueue(void);
    DescriptorsQueue(int in_nFd);
    ~DescriptorsQueue();

    void AddDescriptor(int in_nFd);
    int GetDescriptor(void);
};

}
