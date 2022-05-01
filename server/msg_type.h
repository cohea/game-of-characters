#pragma once

#define MAXNAME_LEN 20

typedef struct msg_header
{
	/*消息类型*/
	char msg_type;
	/*消息长度*/
	int msg_len;
	/*可变长度的消息内容,虚拟的消息内容指针，不占空间，仅仅是一个占位符*/
	char msg_data[0];
}MSG_HDR;

/*所有的消息类型 msg_type*/
/*用户的登录消息*/
#define MSG_LOGIN		0x01
/*登录回复类型*/
#define LOG_RESP_OK		0x02
#define LOG_RESP_FAIL 	0x03

/*端到端转发失败*/
#define 	CTC_FAILED	0x04
//发起对战
#define MSG_CLG			0x05
/*挑战回复*/
#define CLG_RESP_NO		0x06
#define CLG_RESP_OK		0x07


//游戏开始
#define START			0x08
//游戏结束
#define GAMEOVER		0x09
//游戏信息
#define MSG_GAME		0x0a
/*用户聊天消息*/
#define MSG_CHAT		0x0b
//通知信息
#define INFORM			0x0c
/*退出登录消息*/
#define MSG_LOGOUT		0x0d

#define USELESS			0x0e


/*typedef struct chat_msg {
	char name[MAXNAME_LEN];
	int msglen;
	char msg[0];

}CHAT_MSG;*/
//用户登录信息
typedef struct login_msg {
	char name[MAXNAME_LEN];
}LOG_MSG;

//游戏反馈信息
typedef struct game_msg_resp {
	char p1choice;
	char p2choice;
	int p1hp;
	int p2hp;
}GAME_MSG_RESP;

//用户对战选择信息
typedef struct game_msg {
	char choice;
}GAME_MSG;

//端到端信息
typedef struct c_to_c_msg {

	char p1_name[MAXNAME_LEN];
	char p2_name[MAXNAME_LEN];
	int msglen;
	char msg[0];

}CTC_MSG;

