#ifndef USR_MGT_H
#define USR_MGT_H

#include<stdio.h>
#include<string.h>
#include <pthread.h>
#define VISITOR		0
#define ONLINE 		1
#define OFFLINE		3
#define INGAME		2
#define MAX_ULEN 	20		//用户名最大长度 
#define MAX_UNUM 	40		//最多用户数

//定义用户信息格式
typedef struct userinfo{
	//自己的套接字
	int fd;
	char state;
	char choice;
	int points;
	
	char userName[MAX_ULEN];
	
}userInfo;
//存储用户信息列表
struct  userlist {
	int cnt;
	pthread_mutex_t ullock;
	userInfo users[MAX_UNUM];
};
struct usersforout {
	int cnt;
	char usersname[MAX_UNUM][MAX_ULEN];
	char states[MAX_UNUM];
	int points[MAX_UNUM];
};

//初始化用户列表
struct userlist*  userlistcreate();
//添加用户，成功返回1，否则返回0
int user_add(userInfo* user, struct userlist* ul);
//查找用户
int user_find_byname(char* name, struct userlist* ul);
//根据套接字获得用户序号
int user_find_byfd(int fd, struct userlist* ul);

struct usersforout* get_online_users(struct userlist* ul);
#endif