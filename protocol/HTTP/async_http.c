/*
作用:
1. 模拟http协议
2. 使用thread、epoll等技术实现异步访问
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>       /* close */
#include <netdb.h> 

#include <sys/epoll.h>
#include <pthread.h>

#ifdef	__cplusplus
extern "C"
#endif


// 请求头
#define HTTP_VERSION    "HTTP/1.1"
#define USER_AGENT      "User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:10.0.2) Gecko/20100101 Firefox/10.0.2\r\n"
#define ENCODE_TYPE     "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
#define CONNECTION_TYPE "Connection: close\r\n"


#define BUFFER_SIZE     4096



// 将网络地址转化成IP字符串
char *host_to_ip(const char *hostname) {

	// 获取主机地址
	struct hostent *host_entry = gethostbyname(hostname); //gethostbyname_r / getaddrinfo
	if (host_entry) {
		// 将网络地址转化成IP字符串 
		return inet_ntoa(*(struct in_addr*)*host_entry->h_addr_list);
	}
	else {
		return NULL;
	}
}

// 创建套接字
int http_create_socket(char *ip) {
	// 套接字
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// 设置套接字
	struct sockaddr_in sin = { 0 };
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(80);  // 主机字节转化成网络
	sin.sin_family = AF_INET;

	if (-1 == connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))) {
		return -1;
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	return sockfd;

}


// 发送请求
char *http_send_request(int sockfd, const char *hostname, const char *resource) {

	char buffer[BUFFER_SIZE] = { 0 };

	int len = sprintf(buffer,
		"GET %s %s\r\n\
Host: %s\r\n\
%s\r\n\
\r\n",
resource, HTTP_VERSION,
hostname,
CONNECTION_TYPE
);
	printf("request buffer:%s\n", buffer);

	// 发送数据
	send(sockfd, buffer, strlen(buffer), 0);

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	fd_set fdread;
	FD_ZERO(&fdread);
	FD_SET(sockfd, &fdread);

	char *result = (char *)malloc(sizeof(int));
	result[0] = '\0';

	while (1) {
		// 等待服务器响应
		int selection = select(sockfd + 1, &fdread, NULL, NULL, &tv);

		if (!selection || !(FD_ISSET(sockfd, &fdread))) {
			break;
		}
		else {
			len = recv(sockfd, buffer, BUFFER_SIZE, 0);
			if (len == 0) {
				break;
			}

			result = (char *)realloc(result, (strlen(result) + len + 1) * sizeof(char));
			strncat(result, buffer, len);
		}
	}

	return result;
}

// 发送请求
int http_client_commit(const char *hostname, const char *resource) {
	// 将host转化成ip
	char *ip = host_to_ip(hostname);
	// 获取主机ip
	int sockfd = http_create_socket(ip);
	// 发送请求
	char *content = http_send_request(sockfd, hostname, resource);
	if (content == NULL) {
		printf("have no data\n");
	}

	puts("============ http_client_commit:content ============");
	puts(content);

	// 关闭socket
	close(sockfd);
	// 释放http接收值
	free(content);
	return 0;
}

// http请求结构体
struct http_request {
	char *hostname;
	char *resource;
};

// 请求位置
struct http_request reqs[] = {
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=beijing&language=zh-Hans&unit=c" },
	// /*
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=changsha&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=shenzhen&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=shanghai&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=tianjin&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=wuhan&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=hefei&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=hangzhou&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=nanjing&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=jinan&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=taiyuan&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=wuxi&language=zh-Hans&unit=c" },
	{"api.seniverse.com", "/v3/weather/now.json?key=0pyd8z7jouficcil&location=suzhou&language=zh-Hans&unit=c" },
	// */
};

// ============异步实现============

// async_context 上下文
struct async_context {
	pthread_t threadid; // 线程
	int epfd;  // epoll id
};

#define HOSTNAME_LENGTH     1024
#define ASYNC_CLIENT_NUM    1024
// 访问服务器的回调函数定义
typedef void(*async_result_cb)(
	const char *hostname,  // 服务器的返回
	const char *result     // 结果
	);

// epoll体
struct ep_arg {
	int sockfd;
	char hostname[HOSTNAME_LENGTH];
	async_result_cb cb;
};

// 创建线程的回调函数
void *http_async_client_callback(void *arg) {
	/* 轮询查找
	*/
	// 获取上下文
	struct async_context *ctx = (struct async_context*)arg;
	// 获取epoll的id
	int epfd = ctx->epfd;

	while (1) {

		struct epoll_event events[ASYNC_CLIENT_NUM] = { 0 };

		int nready = epoll_wait(epfd,
			events,
			ASYNC_CLIENT_NUM,
			-1
		);

		// 
		if (nready < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			}
			else {
				break;
			}
		}
		else if (nready == 0) {
			continue;
		}

		// 
		printf("nready:%d\n", nready);
		int i = 0;
		for (i = 0; i < nready; i++) {

			struct ep_arg *data = (struct ep_arg*)events[i].data.ptr;
			int sockfd = data->sockfd;

			char buffer[BUFFER_SIZE] = { 0 };
			struct sockaddr_in addr;
			size_t addr_len = sizeof(struct sockaddr_in);

			int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
			data->cb(data->hostname, buffer); //call cb

			// 操作完io后，需要清空状态
			int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
			printf("epoll_ctl DEL --> sockfd:%d\n", sockfd);

			// 释放内存
			close(sockfd);
			free(data);
		}

	}

}

// 初始化函数
struct async_context *http_async_client_init(void) {
	// 创建结构体
	struct async_context *ctx = (struct async_context*)malloc(sizeof(struct async_context));
	if (NULL == ctx) {
		printf("err, http_async_client_init\n");
		return NULL;
	}
	// 创建epoll
	// epoll用于管理多个io，实现io多路复用
	ctx->epfd = epoll_create(1);  // 参数大于0就行
	// 创建线程
	// 编译时添加 -lpthread
	int ret = pthread_create(&ctx->threadid,  // 线程id
		NULL, // 线程属性,栈的大小\属性
		http_async_client_callback, // 回调函数
		ctx
	);
	if (ret) {
		perror("err, pthread_create failed\n");
		// 创建失败时,释放内存
		close(ctx->epfd);
		free(ctx);
		ctx = NULL;
		// return NULL;
	}

	/* 创建子线程之后,一般会是继续执行主线程
	如果此时内核刚好执行完毕主线程的时间片,才会先执行子线程
	*/
	usleep(1);  // 休眠主线程,执行子线程,开始监测io

	return ctx;
}


// 释放内存
int http_asynClient_uninit(struct async_context *ctx) {
	// 释放内存
	pthread_cancel(ctx->threadid);
	close(ctx->epfd);
	free(ctx);

	return 0;
}

// http提交
int http_async_client_commit(
	struct async_context *ctx,  // 上下文
	const char *hostname,   // 访问的目标服务器
	const char *resource,   // 目标服务器的资源
	async_result_cb cb      // 回调函数
) {
	// 1. 发送请求
	char *ip = host_to_ip(hostname);
	if (ip == NULL) { return -1; }

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sin = { 0 };
	sin.sin_addr.s_addr = inet_addr(ip);
	sin.sin_port = htons(80);
	sin.sin_family = AF_INET;

	if (-1 == connect(sockfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in))) {
		return -1;
	}

	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	char buffer[BUFFER_SIZE] = { 0 };

	int len = sprintf(buffer,
		"GET %s %s\r\n\
Host: %s\r\n\
%s\r\n\
\r\n",
resource, HTTP_VERSION,
hostname,
CONNECTION_TYPE
);

	printf("[request buffer]:%s\n", buffer);
	int slen = send(sockfd, buffer, strlen(buffer), 0);

	// 创建epoll
	struct ep_arg *eparg = (struct ep_arg*)calloc(1, sizeof(struct ep_arg));
	if (eparg == NULL) return -1;
	eparg->sockfd = sockfd;
	eparg->cb = cb;

	struct epoll_event ev;
	ev.data.ptr = eparg;
	ev.events = EPOLLIN;

	int ret = epoll_ctl(
		ctx->epfd,      // epoll 的id
		EPOLL_CTL_ADD,
		sockfd,
		&ev
	);

	return ret;
}


int test() {
	// 请求数量
	int count = sizeof(reqs) / sizeof(reqs[0]);
	int i = 0;
	// 发送请求
	for (i = 0; i < count; i++) {
		// 发送请求
/*
	hostname : "api.seniverse.com",
	resource : "/v3/weather/now.json?key=0pyd8z7jouficcil&location=beijing&language=zh-Hans&unit=c"
*/
		http_client_commit(reqs[i].hostname, reqs[i].resource);
	}

	return 0;
}


static void http_async_client_result_callback(const char *hostname, const char *result) {
	printf("hostname:%s, result:%s\n\n\n\n", hostname, result);

}

int main(int argc, char *argv[]) {
	struct async_context *ctx = http_async_client_init();
	if (ctx == NULL) return -1;

	int count = sizeof(reqs) / sizeof(reqs[0]);
	int i = 0;
	for (i = 0; i < count; i++) {
		http_async_client_commit(ctx, reqs[i].hostname, reqs[i].resource, http_async_client_result_callback);
	}

	getchar();
}