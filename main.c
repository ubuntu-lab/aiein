/*server*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sqlite3.h>
#include <signal.h>
#include <time.h>

#define N 128

#define R 1  //register
#define L 2  //login
#define O 3  //logout
#define M 4  //modify
#define Q 5  //query
#define H 6  //history record
#define DATABASE "my.db"

typedef struct {

	int  type;      //类型
	char name[N];   //用户名
	char data[256]; //密钥
	int  flag;      //标识位

}MSG;
//客户端请求处理函数
int do_client(int rws, sqlite3 *db);
//注册处理函数
int do_register(int rws, MSG *msg, sqlite3 *db);
//登录处理函数
int do_login(int rws, MSG *msg, sqlite3 *db);
//注销处理函数
int do_logout(int rws, MSG *msg, sqlite3 *db);
//修改信息函数
int do_modify(int rws,MSG *msg,sqlite3 *db);
//查询单词函数
int do_query(int rws,MSG *msg,sqlite3 *db);
//显示查询记录函数
int do_record(int rws,MSG *msg,sqlite3 *db);


// ./server 192.168.30.202 8888
int main(int argc, const char *argv[])
{

	int s = -1;  //监听套接字
	int rws =-1; //读写套接字
	struct sockaddr_in addr;
	char *errmsg;
	pid_t pid;
	//命令行传参	
	if(argc != 3){
		printf("Usage: %s <ipstr> <port>.\n", argv[0]);
		return -1;
	}	

	//1.创建套接字
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		return -1;
		//	exit(-1);
	}

	//2.初始化服务器端地址结构体
	//bzero(&addr, sizeof(addr));
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = htons(atoi(argv[2]));

	//3.绑定IP和端口
	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		goto ERR_STEP;
		//return -1;
		//exit(-1);
	}

	//4.监听套接字
	if(listen(s, 10) < 0)
	{
		perror("listen");
		goto ERR_STEP;
		//exit(-1);
	}

	//5.设置套接字端口复用属性
	int on = 1;
	//setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
	{
		perror("setsockopt");
		return -1;
	}
	//创建打开数据库
	sqlite3 *db;
	if(sqlite3_open(DATABASE, &db) != SQLITE_OK)
	{
		printf("%s\n", sqlite3_errmsg(db));
		return -1;
	}
	else
	{
		printf("open Database success\n");
	}
	//需要在数据库中创建两张表：学生信息表和查询记录表
	//创建学生信息表
	//char sql[256]="create table if not exists stu(id integer primary key autoincrement, name text,passwd text);";
	char sql[256]="create table if not exists stu(name text,passwd text);";
	printf("sql=%s\n",sql);//调试信息
	if(sqlite3_exec(db,sql,NULL,NULL,&errmsg)!=0)
	{
		fprintf(stderr,"create table fail! %s\n",errmsg);
		return -1;
	}
	//创建历史记录信息表
	char sql_history[256]="create table if not exists record(name text,datetime text,word text);";
	printf("sql_history=:%s\n",sql_history);//调试信息
	if(sqlite3_exec(db,sql_history,NULL,NULL,&errmsg)!=0)
	{
		fprintf(stderr,"create table fail! %s\n",errmsg);
		return -1;
	}
	printf("-------------------准备事项均已成功创建！-------------------------\n");
	printf("-------------------接下来等待客户链接请求-------------------------\n");

	//父进程接收子进程的退出信号
	signal(SIGCHLD, SIG_IGN);

	//6.循环处理客户端的链接服务请求
	while(1)
	{
		printf("wait for client...\n");
#if 0
		/*如需打印连接的客户端信息，则开启此段代码*/
		struct sockaddr_in cliaddr;
		socklen_t clilen=sizeof(cliaddr);
		rws=accept(s,(struct sockaddr*)&cliaddr,&clilen);
		//if(0 >(rws=accept(s,(struct sockaddr*)&cliaddr,&clilen));
		char buf[INET_ADDRSTRLEN]={};
		if(inet_ntop(AF_INET,&cliaddr.sin_addr.s_addr,buf,INET_ADDRSTRLEN))
		{
			printf("client ip:%s ",buf);
		}
		printf("port:%u\n",ntohs(cusaddr.sin_port));
#endif
#if 1
		if((rws = accept(s, NULL, NULL)) < 0)
		{
			perror("accept");
			exit(-1);
		}
#endif
		//调用fork函数创建子进程
		if((pid = fork()) < 0)
		{
			perror("fork");
			exit(-1);
		}
		else if(pid == 0)
		{
			//子进程负责处理客户端发来的链接请求
			close(s);                       //不需要监听套接字,关闭
			do_client(rws, db);             //处理函数,传参读写套接字以及数据库句柄

		}
		else
		{

			//父进程负责接收客户请求
			close(rws);                    //关闭读写套接字
		}
	}
ERR_STEP:
	close(s);
	close(rws);
	return 0;
}
//处理客户端请求函数
int do_client(int rws, sqlite3 *db)
{
	MSG msg;                              //定义信息结构体变量
	//客户端先发送链接请求信息
	//服务器接收客户端的请求信息,并按照双方协议,判断客户端请求的类型
	while(recv(rws, &msg, sizeof(msg), 0) > 0)
	{
		//根据结构体中请求类型变量,调用响应的子程序进行处理
		switch(msg.type)
		{
		case R:
			do_register(rws, &msg, db);   //注册,传参读写套接字,结构体地址,数据库句柄
			break;
		case L:
			do_login(rws, &msg, db);      //登录,传参读写套接字,结构体地址,数据库句柄
			break;
		case O:
			do_logout(rws,&msg,db);       //注销
			break;
		case M:
			do_modify(rws,&msg,db);       //修改
			break;
		case Q:
			do_query(rws,&msg,db);        //查询
			break;
		case H:
			do_record(rws,&msg,db);       //回查
			break;
		default:                          //其他
			printf("Invalid msg.\n");
			break;
		}
	}
	printf("Client is leave...\n");
	close(rws);
	exit(0);                              //退出时，发送SIGCHLD信号，父进程设置忽略处理
}
//注册处理函数
int  do_register(int rws, MSG *msg, sqlite3 *db)
{
	char * errmsg;
	char sql[128];
	//查重
	//char *errmsg;
	int nrow, ncloumn;                         //数据库行数和列数
	char **resultp;                            //定义二级指针保存SOL语句执行的结果
	//拼接SOL语句,查询用户名和密码与数据库内容是否匹配
	sprintf(sql, "select * from stu where name=\'%s\';", msg->name);
	printf("%s\n", sql);                        //调试语句,打印SQL语句信息
	//无回调函数版的,返回调用一个命令的整个结果集
	if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg) != SQLITE_OK)
	{
		printf("%s\n", errmsg);
		return -1;
	}
	else
	{
		printf("get_table ok!\n");
	}

	//调用传参行数,判断是否查找到用户信息,若有则代表已注册,否则可以注册
	if(nrow == 1)
	{
		strcpy(msg->data, "REPEAT");
		send(rws, msg, sizeof(MSG), 0);          //发送匹配成功信息,证明有重复账号
		return 1;
	}
	else
	{
		//拼接SQL语句,将客户端的姓名和密码加入添加到数据库
		sprintf(sql, "insert into stu values('%s', %s);", msg->name, msg->data);
		printf("%s\n", sql);                     //调试语句,打印sql语句内容
		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)//便捷函数返回异常,代表注册失败,注册名已存在
		{
			printf("%s\n", errmsg);
			strcpy(msg->data, "invaild input or stu name already exist.");  //将注册错误信息填写至结构体信息成员,待返回客户端
		}
		else
		{
			printf("client register ok!\n");     //成功
			strcpy(msg->data, "ok!");            //成功信息填写至结构体信息成员,待返回客户端
		}
		if(send(rws, msg, sizeof(MSG), 0) < 0)   //发送信息至客户端,待客户反馈
		{
			perror("send");
			return -1;
		}
	}
	return 0;
}
//登录处理函数
int do_login(int rws, MSG *msg, sqlite3 *db)
{
	char sql[128] = {};
	char *errmsg;
	int nrow, ncloumn;                           //数据库行数和列数
	char **resultp;                              //定义二级指针保存SOL语句执行的结果
	//拼接SOL语句,查询用户名和密码与数据库内容是否匹配
	sprintf(sql, "select * from stu where name='%s' and passwd='%s';", msg->name, msg->data);
	printf("%s\n", sql);                         //调试语句,打印SQL语句信息
	//无回调函数版的,返回调用一个命令的整个结果集
	if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg) != SQLITE_OK)
	{
		printf("%s\n", errmsg);
		return -1;
	}
	else
	{
		printf("get_table ok!\n");
	}

	//调用传参行数,判断是否查找到用户信息,若有则代表成功,否则输入错误或用户不存在
	if(nrow == 1)
	{
		strcpy(msg->data, "OK");
		msg->flag=1;                           //登录成功,设置标识位为1
		send(rws, msg, sizeof(MSG), 0);        //发送匹配成功信息
		return 1;
	}
	else
	{
		strcpy(msg->data, "usr/passwd wrong.");
		send(rws, msg, sizeof(MSG), 0);        //发送匹配失败信息
	}

	return 0;
}
//注销处理子函数
int do_logout(int rws, MSG *msg, sqlite3 *db)
{
	char * errmsg;
	char sql[128];
	//拼接SQL语句,以客户端登录姓名为关键字,从数据库中将整条信息删除
	//	sprintf(sql, "insert into usr values('%s', %s);", msg->name, msg->data);
	sprintf(sql, "delete from stu where name='%s';", msg->name);
	printf("%s\n", sql);                       //调试语句,打印sql语句内容
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)       //便捷函数返回异常,代表注销失败
	{
		printf("%s\n", errmsg);
		strcpy(msg->data, "stu name input error.");                     //将注销错误信息填写至结构体信息成员,待返回客户端
	}
	else
	{
		printf("client logout ok!\n");          //成功
		strcpy(msg->data, "ok!");               //成功注销信息,待返回客户端
	}
	if(send(rws, msg, sizeof(MSG), 0) < 0)      //发送信息至客户端,待客户反馈
	{
		perror("send");
		return -1;
	}
	return 0;
}
//修改信息子函数
int do_modify(int rws,MSG *msg,sqlite3 *db)
{
	char * errmsg;
	char sql[128];

	//拼接SQL语句,将要修改的内容内容传入SQL语句中
	sprintf(sql,"update stu set passwd='%s' where name='%s';",msg->data,msg->name);
	printf("%s\n", sql);//调试语句,打印sql语句内容
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) //便捷函数返回异常,代表修改失败
	{
		printf("%s\n", errmsg);
		strcpy(msg->data, "stu  passwd modify fail.");          //将修改未成功信息填写至结构体信息成员,待返回客户端
	}
	else
	{
		printf("stu passwd modify ok!\n");                      //成功
		strcpy(msg->data, "ok!");                               //成功信息填写至结构体信息成员,待返回客户端
	}
	if(send(rws, msg, sizeof(MSG), 0) < 0)                      //发送信息至客户端,待客户反馈
	{
		perror("send");
		return -1;
	}

	return 0;
}
//查词函数（只负责查找，并返回查询函数结果，由查询函数最终向客户端反馈结果，查词成功还要向数据库插入数据）
int do_searchword(int rws,MSG*msg,char *word)
{
	//打开本地的词典文件,词典打开失败也要通知客户端
	FILE* fp=fopen("./dict.txt","r");
	if(NULL==fp)
	{
		perror("fopen failure");
		strcpy(msg->data,"open dict.txt failure");
		if(0 >send(rws,msg,sizeof(MSG),0))
		{
			perror("send");
			goto ERR_STEP;
			//	return -1;
		}
		//打开成功，则需要比较词典中有无参数中的单词
	}
	printf("word is %s,word len is %ld\n",word,strlen(word));//调试信息
#define MAXSIZE 1024
	char buf[MAXSIZE]={};
	while(1)
	{ 
		while(fgets(buf,MAXSIZE,fp))
		{
			int ret=strncmp(buf,word,strlen(word));
			if(0 > ret)
				continue;                //当前单词比要找单词小，继续
			else if( 0 < ret)            //当前单词比要找单词大，因为单词表是按ASCII码的顺序排序的，后面的单词ASCII更大，所以就是没找到
				goto ERR_STEP;
			//	return -1;
			else if((0==ret) && (buf[strlen(word)]!=' '))  //当前单词和要找单词在前n个字节一致，但词典中该词在长度n后面还有内容，也不是要找的内容
				goto ERR_STEP;
			//	return -1;
			else                         //除以上情况外，就是找到了，找打了就把名词解释发送给客户
			{
				//此处难点是如何找到名词解释在哪里？
				//查看词典格式如下：abandonment      n.  abandoning
				//1.根据单词格式可以看出，名词解释的地址，就是在“单词+空格后的第一个非空字符”的地址
				//2.所以应定义一个指针，最终可以指向这个地址
				//   ①跨过单词
				//   ②跨过空格
				char *ptext;
				ptext=buf+strlen(word);
				while(*ptext==' ')
					ptext++;
				//正文复制到信息结构体中
				strcpy(msg->data,ptext);        
				printf("word find:%s",msg->data); //调试信息
				goto ERR_STEP1;
			}
		}
		if(feof(fp))
			break;
	}
ERR_STEP1:
	fclose(fp);
	return 0;
ERR_STEP:
	fclose(fp);
	return -1;
}
char* gettime()
{
	time_t time_now;
	struct tm *t;
	time_now=time(NULL);
	t=localtime(&time_now);
	return asctime(t);
}

//查询单词函数
int do_query(int rws,MSG *msg,sqlite3 *db)
{
	//接收客户端传来的单词
	//检索该单词是否存在于本地文件或数据库中
	//单词存在:
	//1.则从文件或数据库中取词，放到信息结构体中发送至客户端（关键）
	//2.把该单词和客户端姓名、查询时间信息放到数据库表record中（难点）
	//单词不存在，则发送“单词未找到信息反馈到客户端”
	//鉴于以上查询单词操作的分类情况和复杂度，应单独封装函数进行处理，用返回值区分单词存在还是不存在


	//定义buf接收单词
	char word[256]={};
	char * errmsg;
	char time[256]={};
	strcpy(word,msg->data);
	char sql_message[256]={};
	int ret=do_searchword(rws,msg,word);
	if(0>ret)
	{
		//查询失败信息填入信息结构体
		strcpy(msg->data,"This word cannot be found");
	}
	else
	{
		//拼接，把查询记录添加到record数据表中
		strcpy(time,gettime());
		sprintf(sql_message,"insert into record values('%s','%s','%s');",msg->name,time,word);
		printf("sql_message:%s\n",sql_message);
		if(sqlite3_exec(db,sql_message,NULL,NULL,&errmsg)!=SQLITE_OK)
		{
			printf("%s\n", errmsg);
			strcpy(msg->data, "query success,buf server SQL entry failure");  //将查询信息录入失败信息填写至结构体信息成员,待返回客户端
		}
	//	return -1;
	}
	if(0 > send(rws,msg,sizeof(MSG),0))
	{
		perror("send");
		return -1;
	}
	printf("message:%s\n",msg->data);

	return 0;
}
//回调函数
int record_callback(void *para,int f_num,char **f_value,char **f_name)
{
	int rws;
	MSG msg;
	rws=*(int *)para;
	sprintf(msg.data,"%s  %s  %s",f_value[0],f_value[1],f_value[2]);
	if(0 > send(rws,&msg,sizeof(MSG),0))
	{
		perror("send");
		return -1;
	}
	return 0;
}
//显示查询记录函数
int do_record(int rws,MSG *msg,sqlite3 *db)
{
	char sql[256]={};
	char *errmsg;

	sprintf(sql,"select *from record where name='%s';",msg->name);
	printf("sql=%s\n",sql);
	if(sqlite3_exec(db,sql,record_callback,(void*)&rws,&errmsg)!=SQLITE_OK)
	{
		printf("%s\n",errmsg);
		strcpy(msg->data, "History record query failure");  //将查询信息录入失败信息填写至结构体信息成员,待返回客户端
	}
	else
		printf("query successful.\n");
	msg->data[0]='\0';
	if(0 > send(rws,msg,sizeof(MSG),0))
	{
		perror("send");
		return -1;
	}
	return 0;
}
