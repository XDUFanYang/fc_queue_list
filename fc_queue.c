#include <stdio.h>
#include <stdlib.h>
#include "timers_lib.h"
#include "omp.h" 

struct node_t{
	int value;//TODO: maybe change this
	struct node_t * next;
};

struct queue_t{
	struct node_t * Head;
	struct node_t * Tail;
    int lock;
};

struct lock_list{
    int lock;
};

struct pub_record{ //TODO: pad?? to avoid false sharing.
    long long int temp;
    int pending; //1 = waiting for op
    long long int temp2;
    int op; // operation 1= enqueue , 0= dequeue 链表这里可能要改
    long long int temp3;
    int val;
    long long int temp4;
    int response; //1 means reposponse is here!
    long long int temp5;
};

int ERROR_VALUE=54;

long long int glob_counter=0; 

long long int count_enqs=0;
long long int count_deqs=0;

void lock_queue (struct queue_t * Q){

    while (1){
        if (!Q->lock){
            if(!__sync_lock_test_and_set(&(Q->lock),1)) break;
        }
    }   
}

void unlock_queue(struct queue_t * Q){

    Q->lock = 0;
}

/*让控制结构体初始化 并且将pub进行初始化*/
void initialize(struct queue_t * Q,struct pub_record * pub,int n){//TODO: init count?
	int i;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
	node->next = NULL;
	Q->Head = node; //TODO: check this
	Q->Tail = node;
    Q->lock = 0;
    for(i=0; i <n ;i++){
        pub[i].pending = 0;
        pub[i].response =0;
    }
}


//插入的时候要按照升序插入


void add(struct node_t* L,int val){
    
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
    struct node_t * p=L;
   
    node->value=val;

    //用两个指针解决
    struct node_t * fastp=p->next;

    while(fastp!=NULL){
        if(val<=fastp->value&&val>=p->value){
            p->next=node;
            node->next=fastp;
            return ;
        }
        p=fastp;
        fastp=fastp->next;

    }
    node->next=NULL;
    p->next=node;    

}


/*contain 1返回搜索到 0返回没有搜索到*/
int contain(struct node_t* L,int val){
    struct node_t* p= L;
    while(p->next!=NULL){
        if(p->next->value==val){
            return 1;
        }
        p=p->next;
    }
}


/*在链表中删除一个值为val的节点*/
void remove(struct node_t* L,int val){
    struct node_t* p= L;
    while(p->next!=NULL){
        if(p->next->value==val){
            struct node_t* node= p->next;
            p->next=node->next;
            free(node);
            return;
        }
        p=p->next;
    }
}

/*初始化列表*/
struct node_t * initialize_list(struct pub_record* pub,int n)
{
    //有索引的链表是链表嘛？
    struct node_t * start = (struct node_t *) malloc(sizeof(struct node_t));
    //先不管那个链表是否需要一个索引维护
    //先写一个有头节点的链表
    start->next=NULL;
    start->value=-1000;//用1000这个值来标识头节点

    for(int i=0; i <n ;i++){
        pub[i].pending = 0;
        pub[i].response =0;
    }

    return start;
    
}

/*打印列表*/
void printlist(struct node_t* L){
    struct node_t *p=L;
    
    printf("头节点");
    while(p->next!=NULL){
        printf("->%d",p->next->value);
        p=p->next;
    }

}


void enqueue(struct queue_t * Q, int val){
	
    int store = 0;
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
    node->value =  val;
    node->next = NULL;
    //while(__sync_lock_test_and_set(&Q->lock,1));
    //#pragma omp critical
    
    //struct node_t * tail = Q->Tail;
    //tail->next =node;
    Q->Tail->next=node;
    Q->Tail = node; 
    //__sync_lock_test_and_set(&Q->lock,0);
}

/*
出队操作，将出队的值返回；如果队列为空，返回ERROR_VALUE
*/
int dequeue(struct queue_t * Q, int * val){
    struct node_t * node = Q->Head;
    struct node_t * next = node->next;
    //struct node_t * tail = Q->Tail;
    if(next == NULL) return 0;
    else{
        *val = next->value;
        Q->Head =next;
    } 
    free(node);
    return 1;
           
}

int try_access(struct queue_t * Q,struct pub_record *  pub,int operation, int val,int n){

    int tid=omp_get_thread_num();
    int i,res,count;
    //1
    
    pub[tid].op = operation;
    pub[tid].val = val;
    pub[tid].pending=1;
    while (1){
        //自旋

        if(Q->lock){
            //如果这个线程把锁拿到

            count=0;
            while((!pub[tid].response)&&(count<10000000)) count++; //check periodicaly for lock
            //间隔一段时间

            if(pub[tid].response ==1){
                pub[tid].response=0;
                return (pub[tid].val);
            }
        }
        else{
            
            //type __sync_lock_test_and_set (type * ptr, type value, …)：
            //将ptr设为value并返回ptr操作之前的值。

            if(__sync_lock_test_and_set(&(Q->lock),1)) continue;// must spin backto response
            //如果拿到锁了 就相当于这个操作没有必要执行 continue

            //如果之前没拿到锁 但现在拿到了
            else{
                glob_counter++;
                //glob_counter是干啥的？

                for(i=0 ;i<n; i++){
                    if(pub[i].pending){
                        //如果悬挂 则将悬挂的进程进行操作

                        if (pub[i].op ==1) {count_enqs++; enqueue(Q,pub[i].val);}
                        //如果为入队 那么入队数++ 入队操作
                        else if(pub[i].op==0){
                        //如果出队 那么出队++ 出队操作
                           count_deqs++;
                           res=dequeue(Q,&pub[i].val);
                           if(!res) 
                                    pub[i].val = ERROR_VALUE;
                        }
                        else printf("wtf!!  %d \n",pub[i].op);

                        //操作完以后将该线程解挂
                        pub[i].pending = 0;

                        //操作完后将response置为1 为啥这么做
                        pub[i].response = 1;
                    }
                }

                //将这个拿到线程锁 操作完的线程值返回
                int temp_val=pub[tid].val;

                pub[tid].response=0;//操作完后将response设为0

                Q->lock=0;
                //解开对这个线程锁的锁
                return temp_val;
            }
        }
   }
}

/*这个pub_record的含义？*/
int try_access_list(struct lock_list * Q,struct pub_record * pub,int operation, int val,int n)
{   
    int tid=omp_get_thread_num();
    int i,res,count;
    
    pub[tid].op = operation;//这里需要更改 操作数不一样
    pub[tid].val = val;
    pub[tid].pending=1;

    //自旋
    while (1){
        if(Q->lock){
            //如果这个线程把锁拿到 控制链表的线程锁

            count=0;
            while((!pub[tid].response)&&(count<10000000)) count++; //check periodicaly for lock
            //间隔一段时间

            //如果响应位为1 那么置为0
            if(pub[tid].response==1){
                pub[tid].response=0;
                return (pub[tid].val);//为啥要返回这个val?
            }
        }
        
        else{
            
            //type __sync_lock_test_and_set (type * ptr, type value, …)：
            //将ptr设为value并返回ptr操作之前的值。

            if(__sync_lock_test_and_set(&(Q->lock),1)) continue;// must spin backto response
            //如果拿到锁了 就相当于这个操作没有必要执行 continue

            //如果之前没拿到锁 但现在拿到了
            else{
                glob_counter++;
                //glob_counter是干啥的？ 这里是那个记录拿到锁的次数

                for(i=0 ;i<n; i++){
                    if(pub[i].pending){
                        //如果悬挂 则将悬挂的进程进行操作

                        if (pub[i].op ==1) {count_enqs++; enqueue(Q,pub[i].val);}
                        //如果为入队 那么入队数++ 入队操作
                        else if(pub[i].op==0){
                        //如果出队 那么出队++ 出队操作
                           count_deqs++;
                           res=dequeue(Q,&pub[i].val);
                           if(!res) 
                                    pub[i].val = ERROR_VALUE;
                        }
                        else printf("wtf!!  %d \n",pub[i].op);

                        //操作完以后将该线程解挂
                        pub[i].pending = 0;

                        //操作完后将response置为1 为啥这么做
                        pub[i].response = 1;
                    }
                }

                //将这个拿到线程锁 操作完的线程值返回
                int temp_val=pub[tid].val;

                pub[tid].response=0;//操作完后将response设为0

                Q->lock=0;
                //解开对这个线程锁的锁
                return temp_val;
            }
        }
        

    

    }
   

}


void printqueue(struct queue_t * Q){
    //打印链表

    struct node_t * curr ;
    struct node_t * next ;
    
    curr = Q->Head;
    next = Q->Head->next;
    while (curr != Q->Tail){
        printf("%d ",curr->value);
        curr = next;
        next = curr ->next;
    }
    printf("%d ",curr->value);
    printf("\n");
    
}



// int main(int argc, char *argv[]){

//     int res,val,i,j,num_threads,count,result; 
    
//     num_threads=atoi(argv[1]);
//     count =atoi(argv[2]);


// 	struct queue_t * Q = (struct queue_t *) malloc(sizeof(struct queue_t));

//     struct pub_record pub[num_threads];
//     //Q->Head =  NULL;
//     //Q->Tail =  NULL;
// 	initialize(Q,pub,num_threads);
//     /*result = try_access(Q,pub,1,5,num_threads);
//     result = try_access(Q,pub,1,7,num_threads);
//     result = try_access(Q,pub,0,5,num_threads);
//     printf("asdadsa %d\n",result);
//     result = try_access(Q,pub,1,12,num_threads);
//     printqueue(Q);
//     */
//     /*enqueue(Q,5);
//     enqueue(Q,7);
//     enqueue(Q,4);
//     //res =  dequeue(Q,&val);
//     //if (res) printf("Dequeued %d \n",val);
//     enqueue(Q,1);
//     printqueue(Q)*/
    
//     timer_tt * timer = timer_init();
//     timer_start(timer);
//     #pragma omp parallel for num_threads(num_threads) shared(Q,pub) private(res,val,i,j)
//     //#pragma omp parallel for是OpenMP中的一个指令，表示接下来的for循环将被多线程执行，
//     //另外每次循环之间不能有关系。

//     //那个num_threads(x)说明 用x个线程并发
//     //那个shared() 指定一个或多个变量为线程共享
//     //private()  指定每个线程都有它自己的变量私有副本。
//     for(i=0;i<num_threads;i++){
//         for(j=0; j<count/num_threads;j++){
//                 try_access(Q,pub,1,i,num_threads);
//                 res=try_access(Q,pub,0,9,num_threads);
//                 if(res==ERROR_VALUE) printf("%d\n",res);
            
//         }
//     }

//     timer_stop(timer);
//     double timer_val = timer_report_sec(timer);
//     printf("num_threads %d enq-deqs total %d\n",num_threads,count);
//     printf("thread number %d total time %lf\n",omp_get_thread_num(),timer_val);
//     printf("glob counter %ld \n",glob_counter);

//     timer_tt * timer2=timer_init();
//     timer_start(timer2);
//     int k=0;
//     for(k=0;k<glob_counter;k++){
//         for(i=0;i<num_threads;i++)
//             res=pub[i].pending;
//     }
//     timer_stop(timer2);
//     printf("total delay %lf\n",timer_report_sec(timer2));
//     printqueue(Q);

//     printf("total enqs %ld\n",count_enqs);
//     printf("total deqs %ld\n",count_deqs);

    
//     //------------------------------------------------------
//     /*double thread_time;
//     timer_tt * timer;
//     #pragma omp parallel for num_threads(num_threads) shared(Q) private(timer,j,i,thread_time)
//     for(i=0;i<num_threads;i++){    
//         timer = timer_init();
//         timer_start(timer);
//         lock_queue(Q);
//         for( j=0; j<100;j++)
//             printf("thread %d in critical \n",omp_get_thread_num());
//         unlock_queue(Q);
//         timer_stop(timer);
//         thread_time = timer_report_sec(timer);
//         printf("thread %d  time %lf\n",omp_get_thread_num(),thread_time);
//     }
//     */
// 	return 1;
// }
	
