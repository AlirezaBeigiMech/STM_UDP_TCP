/*
 * tcpserver.c
 *
 *  Created on: Apr 20, 2022
 *      Author: controllerstech.com
 */


#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"


#include "tcpserver.h"
#include "string.h"
static struct netconn *conn, *newconn;
static struct netbuf *buf;
static ip_addr_t *addr;
static unsigned short port;
char msg1[100];
char smsg1[200];

static void tcp_thread(void *arg);
static void tcp_socket_thread(void *arg);


/**** Send RESPONSE every time the client sends some data ******/
static void tcp_thread(void *arg)
{
	err_t err, accept_err, recv_error;

	/* Create a new connection identifier. */
	conn = netconn_new(NETCONN_TCP);


	if (conn!=NULL)
	{
		/* Bind connection to the port number 7. */
		err = netconn_bind(conn, IP_ADDR_ANY, 7);

		if (err == ERR_OK)
		{
			/* Tell connection to go into listening mode. */
			netconn_listen(conn);

			while (1)
			{


				/* Process the new connection. */
				if (netconn_accept(conn, &newconn) == ERR_OK)
				{


					/* receive the data from the client */
					while (netconn_recv(newconn, &buf) == ERR_OK)
					{
						/* Extrct the address and port in case they are required */
						addr = netbuf_fromaddr(buf);  // get the address of the client
						port = netbuf_fromport(buf);  // get the Port of the client

						/* If there is some data remaining to be sent, the following process will continue */
						do
						{

							strncpy (msg1, buf->p->payload, buf->p->len);   // get the message from the client

							// Or modify the message received, so that we can send it back to the client
							int len = sprintf (smsg1, "\"%s\" was sent by the TCP Server\n", msg1);

							netconn_write(newconn, smsg1, len, NETCONN_COPY);  // send the message back to the client
							memset (msg1, '\0', 100);  // clear the buffer
						}
						while (netbuf_next(buf) >0);

						netbuf_delete(buf);
					}

					/* Close connection and discard connection identifier. */
					netconn_close(newconn);
					netconn_delete(newconn);
				}
			}
		}
		else
		{
			netconn_delete(conn);
		}
	}
}


#define SOCK_TARGET_HOST  "169.254.9.77"
#define SOCK_TARGET_PORT  88
char smsg2[1024]; // Adjust size for outgoing message as needed



//struct timeval timeout = {0};
struct timeval timeout = {.tv_sec = 20, .tv_usec = 0};
//timeout->tv_sec = 20; //set recvive timeout = 20(sec)

struct iovec iov[1];

struct msghdr msg_str = {
    .msg_name = NULL,
    .msg_namelen = 0,
    .msg_iov = iov,
    .msg_iovlen = 1,
    .msg_control = NULL,
    .msg_controllen = 0,
    .msg_flags = 0
};


static void tcp_socket_thread(void *arg)
{

	err_t err, accept_err, recv_error;

	/* Create a new connection identifier. */
	//conn = netconn_new(NETCONN_TCP);
	int s,new_s;
	int ret;
    //const size_t BUFFER_SIZE = 1024;
    //char buffer_lwip[BUFFER_SIZE];
	u32_t opt;
	int optval = 1;
	int readed;

	struct msghdr mhdr;
	struct iovec iov[1];
	struct cmsghdr *cmhdr;
	char control[100];
	struct sockaddr_in sin;
	char databuf[150];
	unsigned char tos;

	mhdr.msg_name = &sin;
	mhdr.msg_namelen = sizeof(sin);
	mhdr.msg_iov = iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = &control;
	mhdr.msg_controllen = sizeof(control);
	iov[0].iov_base = databuf;
	iov[0].iov_len = sizeof(databuf);
	memset(databuf, 0, sizeof(databuf));


	/* ################################   */
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);


	  memset(&client_addr, 0, sizeof(client_addr));
	  client_addr.sin_len = sizeof(client_addr);
	  client_addr.sin_family = AF_INET;


	  client_addr.sin_port = htons(7);

	  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	  /* ################################   */
	struct sockaddr_in addr;

	socklen_t addr_len = sizeof(addr);


	  memset(&addr, 0, sizeof(addr));
	  addr.sin_len = sizeof(addr);
	  addr.sin_family = AF_INET;


	  addr.sin_port = htons(9);

	  addr.sin_addr.s_addr = htonl(INADDR_ANY);




	s = lwip_socket(AF_INET, SOCK_STREAM, 0);


		err = lwip_bind(s, (struct sockaddr*)&client_addr, sizeof(client_addr));

		socklen_t len = sizeof(timeout);

		// Put the connection into LISTEN state

		if ( (lwip_listen( s, 5 )) < 0 ) {
					LWIP_DEBUGF( IPERF_DEBUG, ("[%s:%d] \n", __FUNCTION__, __LINE__ ));
					//break;
		}

		if (err == ERR_OK)
		{


			/* Extrct the address and port in case they are required */


			while (1){
				new_s = lwip_accept(s, (struct sockaddr*)&addr, &addr_len);
				if(new_s!=-1){
					LWIP_DEBUGF( IPERF_DEBUG, ("[%s:%d] Accept... (sockfd=%d)\n", __FUNCTION__, __LINE__, connfd ));
					if ( lwip_setsockopt( new_s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, len ) < 0 ) {
					     LWIP_DEBUGF( IPERF_DEBUG, ("Setsockopt failed - cancel receive timeout\n" ));
					 }



					readed = recvmsg(new_s, &mhdr, 0);


					if(readed <= 0){

						lwip_close(new_s);
					}

					else
					{

							sendmsg(new_s,&mhdr, 0);

					}


					netbuf_delete(buf);
				};

				lwip_close(new_s);

			}



		}
		lwip_close(s);



}



void tcpserver_init(void)
{
  //sys_thread_new("tcp_thread", tcp_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);
  sys_thread_new("tcp_thread", tcp_socket_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);
  //sys_thread_new("tcp_thread", tcp_echo_tsk, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);

}
