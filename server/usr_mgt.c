#include "usr_mgt.h"
#include <stdlib.h>

#define OFFLINE 	2
#define ONLINE		1
#define INGAME		3
#define NONE		0

//初始化用户列表，成功返回0，否则返回-1
struct userlist* userlistcreate(){
	struct userlist* ul = malloc(sizeof(struct userlist));
	if (ul == NULL) {
		perror("userlist create fail");
		return NULL;
	}
	memset(ul, 0, sizeof(struct userlist));
	pthread_mutex_init(&ul->ullock, NULL);
	ul->cnt = 0;

	for(int i=0; i<MAX_UNUM; i++){
		ul->users[i].state = NONE;
		ul->users[i].fd = -1;
		ul->users[i].points = 0;
		ul->users[i].choice = NOTCHOOSE;
		//用户列表锁
		
	}
	return ul;
};

//添加用户，成功返回0，否则返回-1
int user_add(userInfo *user, struct userlist* ul){
	if(ul->cnt<MAX_UNUM){
		ul->users[ul->cnt] = *user;
		ul->cnt++;
		return 0;
	}
	else return -1;
	
};

//查找用户名，找到返回用户ID，否则返回-1
int user_find_byname(char* name, struct userlist* ul){
	for(int i=0; i<ul->cnt; i++){
		if(strcmp(ul->users[i].userName,name)==0)return i;
	}
	return -1;
}
int user_find_byfd(int fd, struct userlist* ul)
{
	for (int i = 0; i < ul->cnt; i++) {
		if (ul->users[i].fd == fd)return i;
	}
	return -1;
}

struct usersforout* get_online_users(struct userlist* ul) {
	struct usersforout* ulon = (struct usersforout*)malloc(sizeof(struct usersforout));
	ulon->cnt = ul->cnt;
	for (int i = 0; i < ul->cnt; i++) {
		ulon->states[i] = ul->users[i].state;
		strcpy(ulon->usersname[i], ul->users[i].userName);
		ulon->points[i] = ul->users[i].points;
	}
	return ulon;
}
