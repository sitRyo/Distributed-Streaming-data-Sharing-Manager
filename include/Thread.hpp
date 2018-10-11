/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Thread.hpp
 * Author: hiroaki
 *
 * Created on November 21, 2017, 8:21 AM
 */

#ifndef _THREAD_HPP_
#define _THREAD_HPP_

#include <pthread.h>
#include <cstdlib>


class Thread {
private:
    pthread_t thread;
    void *args;

public:
    Thread(){}
    virtual ~Thread() {}
    virtual void* run(void *arg) = 0;
    
    static void* wrap(void *arg) {
        Thread* pt = (Thread*)arg;
        return pt->run(pt->args);
    }
    void start(void *arg) {
        this->args = arg;
        if (pthread_create(&this->thread, NULL, wrap, this)) {
            printf("thread create error\n");
            abort();
        }       
    }
    
    void wait() {
        if (pthread_join(this->thread, NULL)) {
            printf("join error");
            abort();
        }
    }
};



#endif /* THREAD_HPP */

