/*client*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define N 128
//双方约定相同的信号定义
#define R 1  //register
#define L 2  //login
#define O 3  //logout
#define M 4  //modify
#define Q 5  //query
#define H 6  //history recond

typedef struct {
	int  type;
	char name[N];
	char data[256];
	int  flag;
}MSG;
//注册子函数
int do_register(int sockfd, MSG *msg)
{
	//输入姓名
	msg->type = R;
	printf("Input name:");
	scanf("%s", msg->name);
	getchar();
	//输入密码
	printf("Input passwd:");
	scanf("%s", msg->data);
	getchar();
	//客户端先发送
	if(send(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to send.\n");
		return -1;
	}
	//客户端后接收
	if(recv(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to recv.\n");
		return -1;
	}
	//没有退出就是注册成功!
	//ok! or usr already exist.
	printf("%s\n", msg->data);

	return 0;
}
//登录子函数
int do_login(int sockfd, MSG *msg)
{
	//输入姓名
	msg->type = L;
	printf("Input name:");
	scanf("%s", msg->name);
	getchar();
	//输入密码
	printf("Input passwd:");
	scanf("%s", msg->data);
	getchar();
	//发送登录信息
	if(send(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to send.\n");
		return -1;
	}
	//接收服务器反馈
	if(recv(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to recv.\n");
		return -1;
	}
	//返回ok代表登录成功
	if(strncmp(msg->data, "OK", 3) == 0)
	{
		printf("Login OK!\n");
		return 1;
	}
	//否则打印错误信息
	else
	{
		printf("%s\n", msg->data);
		return 0;
	}

	return 0;
}
//注销子函数
int do_logout(int sockfd, MSG *msg)
{
	if(msg->flag==0)
	{
		printf("Not logged in yet\n ");
		return -1;
	}
	//输入姓名
	msg->type = O;
#if 0
	printf("Input name:");
	scanf("%s", msg->name);
	getchar();
	//输入密码
	printf("Input passwd:");
	scanf("%s", msg->data);
#endif
	//客户端先发送
	if(send(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to send.\n");
		return -1;
	}
	//客户端后接收
	if(recv(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to recv.\n");
		return -1;
	}
	printf("%s\n", msg->data);

	return 0;
}
//修改子函数
int do_modify(int sockfd, MSG *msg)
{
	if(msg->flag==0)
	{
		printf("Not logged in yet\n ");
		return -1;
	}
	msg->type = M;
#if 0
	printf("Input name:");
	scanf("%s", msg->name);
	getchar();
#endif
#if 1
	//输入密码
	printf("Input newpasswd:");
	scanf("%s", msg->data);
	getchar();
#endif
	//客户端先发送
	if(send(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to send.\n");
		return -1;
	}
	//客户端后接收
	if(recv(sockfd, msg, sizeof(MSG), 0) < 0)
	{
		printf("fail to recv.\n");
		return -1;
	}
	printf("%s\n", msg->data);
	return 0;
}
int do_query(int sockfd,MSG *msg)
{
	msg->type=Q;
	while(1)
	{
		//输入要查询的单词,缓存接收单词
		printf("please input a word:");
		if(!fgets(msg->data,sizeof(msg->data)-1,stdin))
		{
			printf("input error.\n");
			return -1;
		}
		msg->data[strlen(msg->data)-1]='\0';
		if(0==strncmp(msg->data,"&",1))
			break;
		//通过套接字将单词传给客户端（先发）
		if(send(sockfd,msg,sizeof(MSG),0)<0)
		{
			printf("fail to send.\n");
			return -1;
		}
		//接收服务器反馈，打印
		if(recv(sockfd,msg,sizeof(MSG),0)<0)
		{
			printf("fail to recv.\n");
			return -1;
		}
		printf("%s\n",msg->data);
	}
	return 0;
}
int do_recond(int sockfd,MSG*msg)
{ 
	msg->type=H;
	if(send(sockfd,msg,sizeof(MSG),0) <0)
	{
		printf("fail to send.\n");
		return -1;
	}
	while(1)
	{
		if(recv(sockfd,msg,sizeof(MSG),0)<0)
		{
			printf("fail to recv.\n");
			return -1;
		}
		if(msg->data[0]==0)  //按约定退出
			break;            
		printf("%s\n",msg->data);
	}
	return 0;
}
//./client 192.168.30.202 8888
int main(int argc, const char *argv[])
{
	int sockfd = -1;
	struct sockaddr_in sin;
	MSG msg;
	memset(&msg,0,sizeof(msg));
	int n;
	//命令行传参
	if(argc != 3){
		printf("Usage: %s <servip> <port>.\n", argv[0]);
		return -1;
	}
	//1.创建套接字
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(-1);
	}

	//2.初始化服务器地址结构体信息
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(argv[1]);
	sin.sin_port = htons(atoi(argv[2]));

	if(connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("connect");
		exit(-1);
	}
	//循环向服务器端发送客户端链接请求,并把交互设计在客户端,服务器只是对客户端请求的应答
	/* ----------------设计协议: 1.客户端请求:注册,登录,退出-----------------*/
next1:
	while(1)
	{

		printf("*****************************************\n");
		printf("*   1:register     2.login     3:quit   *\n");
		printf("*****************************************\n");
		printf("Please choose:");//选号,按号实现响应功能代码
		char buf[256]={};

		if(fgets(buf,256,stdin))
		{
			int size=strlen(buf);
			printf("len=%d\n",size);
			//	int num=buf[0]-'0';
			//  printf("num=%d\n",num);
			//	buf[strlen(buf)-1]='\0';
			if(strlen(buf)==2)
			{
				int num=buf[0]-'0';
				printf("num=%d\n",num);
				if((!((num >= 1)&&(num <=3)))&&(strlen(buf)!='\n'))
				{
					printf("choose input Invalid,please enter again.\n");
					continue;
				}
			}
			else
			{
				printf("choose input Invalid,please enter again.\n");
				continue;
			}
		}
#if 0
		scanf("%d", &n);
		getchar(); //吃垃圾字符

		if(!((n >= '1')&&(n <='3')))
		{
			printf("choose input Invalid,please enter again.\n");
			continue;
		}
#endif
		switch(buf[0]-'0')
		{
		case 1:
			do_register(sockfd, &msg);      //注册子函数
			break;
		case 2:
			if(do_login(sockfd, &msg) == 1) //登录子函数
			{
				goto next;
			}
			break;
		case 3:                              //退出子函数
			close(sockfd);
			exit(0);
			break;
		default:                            //命令错误
			printf("Invalid data cmd.\n");
		}
	}

next:
	while(1)
	{
		printf("**********************************************\n");
		printf("*   1:logout     2:modify     3:return menu  *\n");
		printf("*          4:query        5:record           *\n");
		printf("**********************************************\n");
		printf("Please choose:");
#if 1
		char buf[256]={0};

		if(fgets(buf,256,stdin))
		{
			int size=strlen(buf);
			printf("len=%d\n",size);
			//	int num=buf[0]-'0';
			//  printf("num=%d\n",num);
			//	buf[strlen(buf)-1]='\0';
			if(strlen(buf)==2)
			{
				int num=buf[0]-'0';
				printf("num=%d\n",num);
				if((!((num >= 1)&&(num <=5)))&&(strlen(buf)!='\n'))
				{
					printf("choose input Invalid,please enter again.\n");
					continue;
				}
			}
			else
			{
				printf("choose input Invalid,please enter again.\n");
				continue;
			}
		}
#endif
#if 0
		scanf("%d", &n);
		getchar();
		if(!((n >= '1')&&(n <='5')))
		{
			printf("choose input Invalid,please enter again.\n");
			continue;
		}
#endif
		switch(buf[0]-'0'){
		case 1:
			do_logout(sockfd, &msg);
			break;
		case 2:
			do_modify(sockfd,&msg);
			break;
		case 3:
			goto next1;
			//	break;
		case 4:
			do_query(sockfd,&msg);
			break;
		case 5:
			do_recond(sockfd,&msg);
			break;
		default:
			printf("Invalid data cmd.\n");
		}
	}
	return 0;
}
