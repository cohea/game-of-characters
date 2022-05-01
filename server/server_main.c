#include "common.h"
#include "msg_type.h"
#include "usr_mgt.h"
#include "threadpool.h"

#define MAX_EVENTS 1024
#define MAXHP 5

//定义传送给子线程信息
typedef struct socketinfo{
	int fd;
	int epfd;
	//对应用户列表的位置
	int pos;
}SocketInfo;
typedef struct players {
	int p1;
	int p2;
}Players;
//用户列表
struct userlist *ul;
//用户列表锁
pthread_mutex_t ullock;


//线程池
ThreadPool* pool;
void* client_thread(void* arg);
int handle_client_msgs(SocketInfo* info, char* buf, char* state);
int send_pkt(int fd, char type, char* buf, int datalen);

void client_logout_handle(SocketInfo* info);

void client_login_handle(LOG_MSG* login, SocketInfo* info);

void client_ctc_handle(int fd,int tfd, char type, CTC_MSG* ctc, int datalen);

void* game_start(void* arg);

void client_game_handle(GAME_MSG* gm, SocketInfo* info);




int main(int argc,char** argv){
	//初始化用户列表
	ul = userlistcreate();
	
	int lfd = tcp_server_listen(SERV_PORT);
	//创建epoll实例
	int epfd = epoll_create(100);
	if(epfd==-1){
		perror("epoll_create failed");
		exit(1);
	}
	struct epoll_event ev;
	ev.events=EPOLLIN | EPOLLET;
	ev.data.fd = lfd;

	//添加监听套接字到epoll树上
	epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	//创建Epoll事件集合用于接收内核通知
	struct epoll_event evs[MAX_EVENTS];

	//创建线程池
	pool = threadPoolCreate(3, 10, 100);
	for(;;){
		int num = epoll_wait(epfd, evs, MAX_EVENTS, -1);
	
		for(int i=0; i<num; i++){
			int fd = evs[i].data.fd;

			if(fd == lfd){
				int cfd = accept(fd, NULL, NULL);
				if(cfd == -1){
					perror("accept error");
					exit(1);
				}
				make_nonblocking(cfd);
				//边沿触发，非阻塞
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = cfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
				
			}
			else{
				SocketInfo* info = (SocketInfo*)malloc(sizeof(SocketInfo));
				info->fd = fd;
				info->epfd = epfd;
				info->pos = -1;
				//为线程池添加任务，通过传入含fd,epfd的结构体指针发送消息或从epoll树上摘除
				threadPoolAdd(pool, client_thread, (void*)info);

			}
		}
	}
	sleep(30);

	threadPoolDestroy(pool);
	return 0;
	
}


void* client_thread(void* arg) {
	SocketInfo* info = (SocketInfo*)arg;
	int fd = info->fd;
	char state;
	int n, index;
	//如果消息来自于一个已连接的套接字,则获取对应用户状态
	pthread_mutex_lock(&ullock);
	if ((index = user_find_byfd(fd, ul)) != -1) {
		//如果找到套接字对应的用户则说明消息来自于一个已登入的用户
		printf("from %d\n", index);
		state = ul->users[index].state;
		info->pos = index;
	}
	else {
		//否则为游客
		printf("from visitor\n");
		state = VISITOR;
	}
	pthread_mutex_unlock(&ullock);

	char buf[BUFFER_SIZE];

	n = recv(fd, buf, BUFFER_SIZE, 0);
	if (n < 0) {
		printf("serv: rcv error, exit thread! n=%d,fd=%d\n", n, fd);
		client_logout_handle(info);
	}
	else if (n == 0) {
		printf("client exit\n");
		client_logout_handle(info);
	}
	else {
		handle_client_msgs(info, buf, &state);
	}
	return;
}

int handle_client_msgs(SocketInfo* info, char* buf, char* state)
{
	int fd = info->fd;
	int epfd = info->epfd;
	GAME_MSG* g;
	LOG_MSG* lg;
	CTC_MSG* ctc;


	MSG_HDR* h = (MSG_HDR*)buf;

	switch (*state)
	{
	case VISITOR:
		if (h->msg_type == MSG_LOGIN) {
			lg = (LOG_MSG*)h->msg_data;
			client_login_handle(lg, info);
		}
		
		break;
	case ONLINE:
		//挑战信息、聊天信息、对战拒绝信息转发
		if (h->msg_type == MSG_CLG || h->msg_type == MSG_CHAT || h->msg_type == CLG_RESP_NO) {
			ctc = (CTC_MSG*)h->msg_data;	
			int i;
			pthread_mutex_lock(&ullock);

			i = user_find_byname((char* )ctc->p2_name, ul);
			int tfd = ul->users[i].fd;
			pthread_mutex_unlock(&ullock);
			
			if (i != -1&& tfd!=-1) {
				
				printf("c%d to c%d\n", info->pos, i);
				send_pkt(tfd, h->msg_type, (char*)ctc, h->msg_len);
			}
			else {
				
				printf("%d CTC_FAILED\n",info->pos);
				send_pkt(fd, CTC_FAILED, NULL, 0);
			}
			

			
		}
		//其他信息处理
		else if (h->msg_type == CLG_RESP_OK) {
			ctc = (CTC_MSG*)h->msg_data;
			Players* ps = (Players*)malloc(sizeof(Players));
			pthread_mutex_lock(&ullock);
			ps->p1 = user_find_byname(ctc->p2_name,ul);
			ps->p2 = info->pos;
			pthread_mutex_unlock(&ullock);
			
			if (pool == NULL) {
				printf("have no thread pool");
			}
			else if (ps->p1 == -1) {
				send_pkt(fd, CTC_FAILED, NULL, 0);
			}
			else {
				threadPoolAdd(pool, game_start, (void*)ps);
			}
			
		}
		else if (h->msg_type == MSG_LOGOUT) {
			client_logout_handle(info);
		}
		else if (h->msg_type == INFORM) {
			printf("get users\n");
			pthread_mutex_lock(&ullock);
			struct usersforout* ulon = get_online_users(ul);
			pthread_mutex_unlock(&ullock);
			send_pkt(fd, INFORM, (char*)ulon, sizeof(struct usersforout));
		}
		else {
			send_pkt(fd, USELESS, NULL, 0);
		}
		break;
	case INGAME:
		if (h->msg_type == MSG_CHAT ) {
			ctc = (CTC_MSG*)h->msg_data;
			int index;
			pthread_mutex_lock(&ullock);
			if (h->msg_type == CLG_RESP_NO) {
				index = user_find_byname(ctc->p1_name, ul);
			}
			else {
				index = user_find_byname(ctc->p2_name, ul);
			}
			int tfd = ul->users[index].fd;
			if (index != -1 && tfd != -1) {
				pthread_mutex_unlock(&ullock);
				printf("c%d to c%d\n", info->pos, index);
				send_pkt(tfd, h->msg_type, (char*)ctc, h->msg_len);
			}
			else {
				pthread_mutex_unlock(&ullock);
				printf("%d CTC_FAILED", info->pos);
				send_pkt(fd, CTC_FAILED, NULL, 0);
			}



		}
		else if (h->msg_type == MSG_GAME) {
			g = (GAME_MSG*)h->msg_data;
			client_game_handle(g, info);
		}
		else if (h->msg_type == MSG_LOGOUT) {
			client_logout_handle(info);
		}
		else if (h->msg_type == INFORM) {
			pthread_mutex_lock(&ullock);
			struct usersforout* ulon = get_online_users(ul);
			pthread_mutex_unlock(&ullock);
			send_pkt(fd, INFORM, (char*)ulon, sizeof(struct usersforout));
		}
		else {
			send_pkt(fd, USELESS, NULL, 0);
		}
		break;
	default:
		break;
	}

	return -1;
}
int send_pkt(int fd, char msgtype, char* data, int datalen) {
	MSG_HDR* spkt = (MSG_HDR*)malloc(sizeof(MSG_HDR) + datalen);
	spkt->msg_len = datalen;
	memcpy(spkt->msg_data, data, datalen);
	spkt->msg_type = msgtype;
	int rc = send(fd, spkt, datalen + sizeof(MSG_HDR), 0);
	free(spkt);
	spkt = NULL;
	return rc;
}

void client_logout_handle(SocketInfo* info) {
	int fd = info->fd;
	int epfd = info->epfd;
	int index = info->pos;
	pthread_mutex_lock(&ullock);
	//用户index为-1，未登录,不为-1则更新用户列表，并登出
	if(index != -1) {
		ul->users[index].state = OFFLINE;
		ul->users[index].fd = -1;
	}
	pthread_mutex_unlock(&ullock);
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
}
//处理用户登录信息
void client_login_handle(LOG_MSG* login, SocketInfo* info) {
	int fd = info->fd;
	int index = info->pos;
	pthread_mutex_lock(&ullock);
	if ((index = user_find_byname(login->name, ul)) == -1) {
		//没有则添加用户并登入,并更新用户状态
		userInfo* newuser = (userInfo*)malloc(sizeof(userInfo));
		newuser->fd = fd;
		newuser->state = ONLINE;
		strcpy(newuser->userName, login->name);
		if (user_add(newuser, ul) == -1) {
			send_pkt(fd, LOG_RESP_FAIL, NULL, 0);
		}
		pthread_mutex_unlock(&ullock);
		free(newuser);
		printf("%s log in.\n", login->name);
		send_pkt(fd, LOG_RESP_OK, NULL, 0);
	}
	//有且离线则登入
	else if (ul->users[index].state == OFFLINE) {
		ul->users[index].state = ONLINE;
		ul->users[index].fd = fd;
		pthread_mutex_unlock(&ullock);
		printf("%s log in.\n", login->name);
		send_pkt(fd, LOG_RESP_OK, NULL, 0);
	}
	else {
		//其余情况都登录失败
		pthread_mutex_unlock(&ullock);
		client_logout_handle(info);
		printf("log in fail.\n");
		send_pkt(fd, LOG_RESP_FAIL, NULL, 0);
	}
}
//转发信息
void  client_ctc_handle(int fd, int tfd,char type, CTC_MSG* ctc, int datalen) {
	if  (tfd != -1) {
		send_pkt(tfd, type, (char*)ctc, datalen);
	}
	else {
		send_pkt(fd, CTC_FAILED, NULL, 0);
	}
}

int compare(char p1_choice, char p2_choice) {
	if (p1_choice == p2_choice) return 0;
	else {
		switch (p1_choice)
		{
		case NOTCHOOSE:
			return -1;
		case SCISSORS:
			if (p2_choice == NOTCHOOSE || p2_choice == PAPER) {
				return 1;
			}
			else return -1;
		case ROCK:
			if (p2_choice == NOTCHOOSE || p2_choice == SCISSORS) {
				return 1;
			}
			else return -1;
		case PAPER:
			if (p2_choice == NOTCHOOSE || p2_choice == ROCK) {
				return 1;
			}
			else return -1;
		default:
			break;
		}
	}
	return -2;
}

void* game_start(void *arg) {
	
	Players* ps = (Players*)arg;
	int p1 = ps->p1;
	int p2 = ps->p2;
	printf("game start between %d and %d\n", p1, p2);
	//游戏开始，血量状态初始化
	void* reval;
	int p1_hp = MAXHP, p2_hp = MAXHP;
	pthread_mutex_lock(&ullock);
	int fd = ul->users[p1].fd;
	int tfd = ul->users[p2].fd;
	ul->users[p1].state = INGAME;
	ul->users[p2].state = INGAME;
	pthread_mutex_unlock(&ullock);
	//通知双方游戏开始
	send_pkt(fd, START, NULL, 0);
	send_pkt(tfd, START, NULL, 0);

	while (p1_hp > 0 && p2_hp > 0) {
		sleep(5);
		pthread_mutex_lock(&ullock);
		char p1_choice = ul->users[p1].choice;
		ul->users[p1].choice = NOTCHOOSE;
		char p2_choice = ul->users[p2].choice;
		ul->users[p2].choice = NOTCHOOSE;
		pthread_mutex_unlock(&ullock);
		int result = compare(p1_choice, p2_choice);
		
		switch (result) {
		case-1:
			p1_hp--;
			break;
		case 1:
			p2_hp--;
			break;
		default:
			break;
		}
		printf("%d,%d\n", p1_hp, p2_hp);
		int len = sizeof(GAME_MSG_RESP);
		GAME_MSG_RESP* gmsg=(GAME_MSG_RESP*)malloc(len);
		gmsg->p1choice = p1_choice;
		gmsg->p1hp = p1_hp;
		gmsg->p2choice = p2_choice;
		gmsg->p2hp = p2_hp;
		if (send_pkt(fd, MSG_GAME, gmsg, len) <= 0) pthread_exit(reval);
		else printf("result sent.\n");
		if (send_pkt(tfd, MSG_GAME, gmsg, len) <= 0) pthread_exit(reval);
		else printf("result sent.\n");
		free(gmsg);
	}
	pthread_mutex_lock(&ullock);
	if(p1_hp<=0){
		ul->users[p2].points++;
	}
	else if(p2_hp <= 0){
		ul->users[p1].points++;
	}
	ul->users[p1].state = ONLINE;
	ul->users[p2].state = ONLINE;
	pthread_mutex_unlock(&ullock);
	sleep(3);

	if (send_pkt(fd, GAMEOVER, NULL, 0) <= 0) pthread_exit(reval);
	else printf("over.\n");
	if (send_pkt(tfd, GAMEOVER, NULL, 0) <= 0) pthread_exit(reval);
	else printf("over.\n");
	
}
void client_game_handle(GAME_MSG* gm, SocketInfo *info) {
	pthread_mutex_lock(&ullock);
	ul->users[info->pos].choice = gm->choice;
	pthread_mutex_unlock(&ullock);
}