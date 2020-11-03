#include <stdio.h>
#include <stdlib.h>
#include "timers_lib.h"

struct node_t{
	int value;//TODO: maybe change this
	struct node_t * next;
};

/*初始化列表 缺少本地记录和线程数 形参注意一下*/
struct node_t * initialize_list()
{
    //有索引的链表是链表嘛？
    struct node_t * start = (struct node_t *) malloc(sizeof(struct node_t));
    //先不管那个链表是否需要一个索引维护
    //先写一个有头节点的链表
    start->next=NULL;
    start->value=1000;//用1000这个值来标识头节点
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


void add(struct node_t* L,int val){
    
    struct node_t * node = (struct node_t *) malloc(sizeof(struct node_t));
    struct node_t * p=L;
   
    node->value=val;

    //用两个指针解决
    struct node_t * fastp=p->next;

    while(fastp!=NULL){
        if(val>=p->value){
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

int main(){
     
    struct node_t * start=initialize_list();
    printlist(start);
    return 0;

}