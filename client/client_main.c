#include"common.h"
#include"msg_type.h"
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <glib.h>
#include "usr_mgt.h"
#define VISITOR 0
#define ONLINE 1
#define INGAME 2

#define OURPORT 43211
gint sd;
struct sockaddr_in s_in;
char* ipaddress=NULL;
static GtkBuilder* newhall_builder=NULL;
static GtkTextView* text=NULL;//对战聊天文本框
static GtkTextView* text2=NULL;//对战聊天文本框
static GtkTextBuffer *buffer=NULL;//文本缓冲区
static GtkTextBuffer *buffer2=NULL;//文本缓冲区
static GtkWidget *name_entry=NULL;//用户名输入框
static GtkEntry *send_entry=NULL;
static GtkWidget *log_in_btn;//登入按钮
static GtkWidget *Paper_btn=NULL;
static GtkWidget *Rock_btn=NULL;
static GtkWidget *Scissors_btn=NULL;

pthread_t rid;
pthread_mutex_t lock;
gchar state;
gchar username[MAXNAME_LEN]={'0'};
gchar opponent[MAXNAME_LEN]={'0'};
gchar buf[BUFFER_SIZE];
int send_pkt(int fd, char msgtype, char* data, int datalen);
void on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean do_connect_run();
void on_button_clicked(GtkButton *button, gpointer data);
void create_win();
void on_destroy(GtkWidget *widget, GdkEvent *event, gpointer data);
void* recvthread(void* arg);
void on_log_in_btn_clicked(GtkWidget *button, gpointer data);
void on_send_btn_clicked(GtkWidget *button, gpointer data);
void on_Rock_btn_clicked(GtkWidget *button, gpointer data);
void on_Paper_btn_clicked(GtkWidget *button, gpointer data);
void on_Scissors_btn_clicked(GtkWidget *button, gpointer data);
void log_out();
void disphelp();

//组装头部和数据段
int send_pkt(int fd, char msgtype, char* data, int datalen) {
	MSG_HDR* spkt = (MSG_HDR*)malloc(sizeof(MSG_HDR) + datalen);
	spkt->msg_len = datalen;
	memcpy(spkt->msg_data, data, datalen);
	spkt->msg_type = msgtype;
	int rc = send(fd, spkt, datalen + sizeof(MSG_HDR), 0);
	if(rc<0){
		g_print("send erro");
	}
	free(spkt);
	return rc;
}
void on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    log_out();
    gtk_main_quit();
}
void create_win()
{
    GtkWidget *win, *box;
    GtkWidget *button;
    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(win), "delete_event", G_CALLBACK(on_destroy), NULL);
    gtk_window_set_title(GTK_WINDOW(win), "输入用户名");
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(win), 10);
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    box = gtk_box_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(win), box);
    name_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), name_entry, TRUE, TRUE, 5);
    button = gtk_button_new_with_label("OK");
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_button_clicked), win);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 5);
    gtk_widget_show_all(win);
}
gboolean do_connect_run()
{	
    struct hostent *host;
    GtkTextIter iter;
    gint slen;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, "打开套接字时出错！\n", -1);
        return FALSE;
    }
    s_in.sin_family = AF_INET;
    host=gethostbyname(ipaddress);
    s_in.sin_addr=*((struct in_addr *)host->h_addr);
    s_in.sin_port = htons(OURPORT);
    slen = sizeof(s_in);
    if (connect(sd, (struct sockaddr*)&s_in, slen) < 0) {
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, "连接服务器时出错！\n", -1);
		pthread_mutex_unlock(&lock);
        return FALSE;
    }
    else {
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, username, -1);
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, "成功与服务器连接...\n", -1);
		
		LOG_MSG* lg=(LOG_MSG*)malloc(sizeof(LOG_MSG));
		pthread_mutex_lock(&lock);
		strcpy(lg->name, username);
		pthread_mutex_unlock(&lock);
        send_pkt(sd,MSG_LOGIN, (char*)lg, sizeof(LOG_MSG));
		if(recv(sd, buf,sizeof(MSG_HDR),0)>0){
			
			MSG_HDR* h=(MSG_HDR*)buf;
			if(h->msg_type==LOG_RESP_OK){
				gtk_text_buffer_get_end_iter(buffer, &iter);
        		gtk_text_buffer_insert(buffer, &iter, "成功登入...\n", -1);
				pthread_mutex_lock(&lock);
				state=ONLINE;
				pthread_mutex_unlock(&lock);
        		return TRUE;
			}
		}
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_buffer_insert(buffer, &iter, "登入失败...\n", -1);
		close(sd);
		return FALSE;
    }
	pthread_mutex_unlock(&lock);
}
//用户名发送按钮被点击
void on_button_clicked(GtkButton *button, gpointer data)
{
    const gchar *name;
    name = gtk_entry_get_text(GTK_ENTRY(name_entry));
	pthread_mutex_lock(&lock);
    sprintf(username, "%s", name);
	pthread_mutex_unlock(&lock);
    if (do_connect_run()) {
        gtk_widget_set_sensitive(log_in_btn, FALSE);
		
        pthread_create(&rid, NULL, recvthread, NULL);
    }
    gtk_widget_destroy(GTK_WIDGET(data));
}
void on_destroy(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_widget_destroy(widget);
}
void on_log_in_btn_clicked(GtkWidget *button, gpointer data)
{
    create_win();
}
void on_send_btn_clicked(GtkWidget *button, gpointer data){
	const char deli[2] = " ";
	char *token;
	pthread_mutex_lock(&lock);
	if(state==VISITOR){
		pthread_mutex_unlock(&lock);
		return;
	} 
	pthread_mutex_unlock(&lock);
	
    gchar *message = gtk_entry_get_text(GTK_ENTRY(send_entry));
    if (strcmp(message, "") == 0){
		return;
	} 
	
	token = strtok(message, deli);
	if(strcmp(token, "chat")==0){
		token = strtok(NULL, deli);
		char name[MAXNAME_LEN];
		strcpy(name,token);
		if(strlen(token)<=0){
			return;
		}

		token = strtok(NULL, deli);
		
		int datalen=strlen(token);
		if(datalen<=0){
			return;
		}
		CTC_MSG* ctc=(CTC_MSG*)malloc(sizeof(CTC_MSG)+datalen);
		pthread_mutex_lock(&lock);
		strcpy(ctc->p1_name, username);
		pthread_mutex_unlock(&lock);
		strcpy(ctc->p2_name, name);
		ctc->msglen = datalen;
		strcpy(ctc->msg, token);
		send_pkt(sd, MSG_CHAT, (char*)ctc, sizeof(CTC_MSG)+datalen);
		free(ctc);
		ctc=NULL;

	}
	else if(strcmp(token, "clg")==0){
		
		token = strtok(NULL, deli);
		char name[MAXNAME_LEN];
		strcpy(name,token);
		if(strlen(token)<=0){
			return;
		}
		else{
			if(strcmp(token,username)==0){
				printf("you can't clg yourself!\n");
				return;
			}
			CTC_MSG* ctc=(CTC_MSG*)malloc(sizeof(CTC_MSG));
			pthread_mutex_lock(&lock);
			strcpy(opponent, name);
			strcpy(ctc->p1_name, username);
			pthread_mutex_unlock(&lock);
			strcpy(ctc->p2_name, name);
			
			
			ctc->msglen = 0;
			send_pkt(sd, MSG_CLG, (char*)ctc, sizeof(CTC_MSG));
			free(ctc);
			ctc=NULL;
		}

	}
	else if(strcmp(token, "OK")==0){
		if(strcmp(opponent,"")==0){
			return;
		}
		CTC_MSG* c2c=(CTC_MSG*)malloc(sizeof(CTC_MSG));
		pthread_mutex_lock(&lock);
		strcpy((char*)c2c->p1_name, username);
		strcpy((char*)c2c->p2_name, opponent);
		pthread_mutex_unlock(&lock);
		
		c2c->msglen = 0;
		send_pkt(sd, CLG_RESP_OK, (char*)c2c, 0);
		free(c2c);
		c2c=NULL;
	}
	else if(strcmp(token, "NO")==0){
		if(strcmp(opponent,"")==0){
			return;
		}
		CTC_MSG* ctc=(CTC_MSG*)malloc(sizeof(CTC_MSG));
		pthread_mutex_lock(&lock);
		strcpy((char*)ctc->p1_name, username);
		strcpy((char*)ctc->p2_name, opponent);
		memset(opponent,'0',MAXNAME_LEN);
		pthread_mutex_unlock(&lock);
		ctc->msglen = 0;
		send_pkt(sd, CLG_RESP_NO, (char*)ctc, 0);
		free(ctc);
		ctc=NULL;
	}
	else if(strcmp(token, "help")==0){
		disphelp();
	}
	else if(strcmp(token, "logout")==0){
		log_out();
	}
	else if(strcmp(token, "get")==0){
		send_pkt(sd, INFORM, NULL, 0);
	}
	else if(strcmp(token, "leave")==0){
		pthread_mutex_lock(&lock);
		state = ONLINE;
		pthread_mutex_unlock(&lock);
	}
    gtk_entry_set_text(GTK_ENTRY(send_entry), "");
}

void on_Rock_btn_clicked(GtkWidget *button, gpointer data){
	GAME_MSG* gmsg = (GAME_MSG*)malloc(sizeof(GAME_MSG));
	gmsg->choice=ROCK;
	if(send_pkt(sd, MSG_GAME, (char*)gmsg, sizeof(GAME_MSG))>0){
		printf("button sent successfully\n");
	}

};
void on_Paper_btn_clicked(GtkWidget *button, gpointer data){
	GAME_MSG* gmsg = (GAME_MSG*)malloc(sizeof(GAME_MSG));
	gmsg->choice=PAPER;
	if(send_pkt(sd, MSG_GAME, (char*)gmsg, sizeof(GAME_MSG))>0){
		printf("button sent successfully\n");
	}

};
void on_Scissors_btn_clicked(GtkWidget *button, gpointer data){
	GAME_MSG* gmsg = (GAME_MSG*)malloc(sizeof(GAME_MSG));
	gmsg->choice=SCISSORS;
	if(send_pkt(sd, MSG_GAME, (char*)gmsg, sizeof(GAME_MSG))>0){
		printf("button sent successfully\n");
	}

};
void log_out(){
	GtkTextIter start,end;
	send_pkt(sd, MSG_LOGOUT, NULL, 0);
	close(sd);
	gtk_widget_set_sensitive(log_in_btn, TRUE);

	pthread_mutex_lock(&lock);
	memset(opponent,'0',MAXNAME_LEN);
	state = VISITOR;
	pthread_mutex_unlock(&lock);

	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_buffer_delete(buffer, &start, &end);
	gtk_text_buffer_get_start_iter(buffer2, &start);
	gtk_text_buffer_get_end_iter(buffer2, &end);
	gtk_text_buffer_delete(buffer2, &start, &end);
}
void disphelp(){
	GtkTextIter iter2;
	gtk_text_buffer_get_end_iter(buffer2, &iter2);
	gtk_text_buffer_insert(buffer2, &iter2, "get:get information of users online\n clg <name>: to challenge sb.\n help:check help information\n chat <name> <message>:to chat with sb.\n logout:log out current user\n", -1);
}

int main(int argc, char** argv) {
	gtk_init(&argc,&argv);
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_lock(&lock);
	state=VISITOR;
	pthread_mutex_unlock(&lock);
	if (argc != 2) {
		error(1, 0, "usage: %s <IPaddress>", argv[0]);
	}
	ipaddress = argv[1];
	newhall_builder = gtk_builder_new();
	gtk_builder_add_from_file(newhall_builder, "newhall.glade", NULL);
	GtkWidget*mainwin;
	mainwin = GTK_WIDGET(gtk_builder_get_object(newhall_builder, "window_main"));
	text=GTK_TEXT_VIEW(gtk_builder_get_object(newhall_builder, "text_view"));
	text2=GTK_TEXT_VIEW(gtk_builder_get_object(newhall_builder, "usersinfo"));
	buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	buffer2=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text2));
	disphelp();
	log_in_btn=GTK_WIDGET(gtk_builder_get_object(newhall_builder, "log_in_btn"));
	Rock_btn=GTK_WIDGET(gtk_builder_get_object(newhall_builder, "Rock_btn"));
	Scissors_btn=GTK_WIDGET(gtk_builder_get_object(newhall_builder, "Scissors_btn"));
	Paper_btn=GTK_WIDGET(gtk_builder_get_object(newhall_builder, "Paper_btn"));
	send_entry =GTK_ENTRY(gtk_builder_get_object(newhall_builder, "cmd"));

	gtk_builder_connect_signals(newhall_builder, NULL);
	g_object_unref(newhall_builder);
    gtk_widget_show_all(mainwin);


    gtk_main();


}

void* recvthread(void* arg) {
	int n;
	GtkTextIter iter;
	GtkTextIter iter2;
	char state1;
	
	
	while (1) {
		
		
		
		n = recv(sd, buf, sizeof(buf), 0);
		pthread_mutex_lock(&lock);
		state1 = state;
		
		printf("now state :%d\n",state1);
		pthread_mutex_unlock(&lock);
		if(n<=0){
			error(1, errno, "recv failed");
			break;
		}
		
		MSG_HDR* h = (MSG_HDR*)buf;
		switch (state1)
		{
		case ONLINE:
			if (h->msg_type == MSG_CHAT) {
				//聊天信息直接显示在对话框
				CTC_MSG*ctc=(CTC_MSG*)h->msg_data;
			
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, (char *)ctc->p2_name, (gint)strlen(ctc->p2_name));
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, " :", -1);
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, (char *)ctc->msg , ctc->msglen);
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, "\n" , -1);		
				
			}
			else if (h->msg_type == MSG_CLG) {
				CTC_MSG*ctc=(CTC_MSG*)h->msg_data;
				pthread_mutex_lock(&lock);
				memcpy(opponent,(char *)ctc->p1_name,MAXNAME_LEN);
				pthread_mutex_unlock(&lock);
				

				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, (char *)ctc->p1_name, strlen((char *)ctc->p1_name));
				
				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, " want to play game with you, OK or NO?\n", -1);

			}
			else if(h->msg_type==CTC_FAILED){
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, "forward failed\n", -1);

			}
			else if(h->msg_type==CLG_RESP_NO){
				//其他信息则忽略
				CTC_MSG*ctc=(CTC_MSG*)h->msg_data;
				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, (char *)ctc->p1_name, strlen((char *)ctc->p1_name));
				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, " refuse your challenge!\n", -1);

			}
			else if(h->msg_type==START){
				CTC_MSG*ctc=(CTC_MSG*)h->msg_data;
				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, " GAME START!\n", -1);
				pthread_mutex_lock(&lock);
				state = INGAME;
				pthread_mutex_unlock(&lock);

			}
			else if(h->msg_type==INFORM){
				struct usersforout* ulon = (struct usersforout*)h->msg_data;
				for(int i=0; i<ulon->cnt; i++){
					char userinfo[30];
					char s[10];
					switch (ulon->states[i])
					{
					case ONLINE:
						sprintf(s,"ONLINE");
						break;
					case INGAME:
				
						sprintf(s,"INGAME");
						break;
					case OFFLINE:
						
						sprintf(s,"OFFLINE");
						break;
					default:
						sprintf(s," ");
						break;
					}
					sprintf(userinfo,"%s, points:%d, state:%s\n",ulon->usersname[i],ulon->points[i],s);
					gtk_text_buffer_get_end_iter(buffer, &iter);
					gtk_text_buffer_insert(buffer, &iter, userinfo , strlen(userinfo));
				}
			}
			else{
				printf("sever don't recognize!\n");
			}
			break;
		case INGAME:
			
			if (h->msg_type == MSG_CHAT) {
				//聊天信息直接显示在对话框
				CTC_MSG*ctc=(CTC_MSG*)h->msg_data;

				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, (char *)ctc->p2_name, (gint)strlen((char *)ctc->p2_name));
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, " :", -1);
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, (char *)ctc->msg , ctc->msglen);
				gtk_text_buffer_get_end_iter(buffer, &iter);
				gtk_text_buffer_insert(buffer, &iter, "\n" , -1);	
			}
			else if(h->msg_type==INFORM){
				struct usersforout* ulon = (struct usersforout*)h->msg_data;
				for(int i=0; i<ulon->cnt; i++){
					char userinfo[30];
					char s[10];
					switch (ulon->states[i])
					{
					case ONLINE:
						sprintf(s,"ONLINE");
						break;
					case INGAME:
				
						sprintf(s,"INGAME");
						break;
					case OFFLINE:
						
						sprintf(s,"OFFLINE");
						break;
					default:
						sprintf(s," ");
						break;
					}
					sprintf(userinfo,"%s, points:%d, state:%s\n",ulon->usersname[i],ulon->points[i],s);
					gtk_text_buffer_get_end_iter(buffer, &iter);
					gtk_text_buffer_insert(buffer, &iter, userinfo , strlen(userinfo));
				}
			}
			else if (h->msg_type == MSG_GAME) {
				//游戏信息回显在游戏信息接收窗口
				
				GAME_MSG_RESP*ginfo=(GAME_MSG_RESP*)h->msg_data;
				switch (ginfo->p1choice)
				{
				case SCISSORS:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p1 choice:\tScissors\n", -1);
					break;
				case ROCK:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p1 choice:\tRock\n", -1);
					break;
				case PAPER:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p1 choice:\tPaper\n", -1);
					break;
				default:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p1 choice:\tNONE\n", -1);
					break;
				};
				
				switch (ginfo->p2choice)
				{
				case SCISSORS:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p2 choice:\tScissors\n", -1);
					break;
				case ROCK:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p2 choice:\tRock\n", -1);
					break;
				case PAPER:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p2 choice:\tPaper\n", -1);
					break;
				default:
					gtk_text_buffer_get_end_iter(buffer2, &iter2);
					gtk_text_buffer_insert(buffer2, &iter2, "p2 choice:\tNONE\n", -1);
					break;
				};
				char str1[30];
				sprintf(str1, "p1 hp: %d\np2 hp: %d\n" , ginfo->p1hp,ginfo->p2hp);
				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, str1,-1);
						
			}
			else if(h->msg_type==GAMEOVER){
				gtk_text_buffer_get_end_iter(buffer2, &iter2);
				gtk_text_buffer_insert(buffer2, &iter2, "GAME OVER!\n",-1);
				pthread_mutex_lock(&lock);
				state = ONLINE;
				pthread_mutex_unlock(&lock);
			}
			
			break;
		default:
			break;
		}
		
	}
}





/*
GtkBuilder* create_win(const char* name);
int send_pkt(int fd, char msgtype, char* data, int datalen) {
	MSG_HDR* spkt = (MSG_HDR*)malloc(sizeof(MSG_HDR) + datalen);
	spkt->msg_len = datalen;
	memcpy(spkt->msg_data, data, datalen);
	spkt->msg_type = msgtype;
	int rc = send(fd, spkt, datalen + sizeof(MSG_HDR), 0);
	if (rc < 0) {
		printf("send error");
		return -1;
	}
	free(spkt);
	return rc;
}
GtkBuilder* create_win(const char* name)
{
	GtkWidget* win;
	GtkBuilder* builder= gtk_builder_new();
	//import an interface file that has been built
	gtk_builder_add_from_file(builder, name, NULL);

    win = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(builder);
    gtk_widget_show_all(win);
	return builder;
}

//login callback fuction
void on_log_in_btn_clicked(GtkButton *button, gpointer data)
{
	name_entry=GTK_WIDGET(gtk_builder_get_object(login,"input_win"));
	int fd = tcp_client(ipaddress, SERV_PORT);
	LOG_MSG* lg = (LOG_MSG*)malloc(sizeof(LOG_MSG));
	strcpy(lg->name, data);
	printf("%s",lg->name);
	send_pkt(fd, MSG_LOGIN, (char*)lg, sizeof(LOG_MSG));
	free(lg);
	lg=NULL;
	
	
}
*/
