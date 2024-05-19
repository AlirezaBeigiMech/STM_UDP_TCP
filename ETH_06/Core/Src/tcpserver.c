/*
 * tcpserver.c
 *
 *  Created on: Apr 20, 2022
 *      Author: controllerstech.com
 */


#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/sys.h"

#include "tcpserver.h"
#include "string.h"
static struct netconn *conn, *newconn;
static struct netbuf *buf;
static ip_addr_t *addr,dest_addr;
static unsigned short port, dest_port;
char msg1[100];
char smsg1[200];

char msgc[100];
char smsgc[200];
int indx = 0;

void tcpsend (char *data);
static void tcp_thread(void *arg);
static void tcpinit_thread(void *arg);

// tcpsem is the binary semaphore to prevent the access to tcpsend
sys_sem_t tcpsem;

void tcpsend (char *data)
{
	// send the data to the connected connection
	netconn_write(conn, data, strlen(data), NETCONN_COPY);
	// relaese the semaphore
	sys_sem_signal(&tcpsem);
}

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

static void tcpinit_thread(void *arg)
{
	err_t err, connect_error;

	/* Create a new connection identifier. */
	conn = netconn_new(NETCONN_TCP);

	osDelay(2000);
	if (conn!=NULL)
	{
		/* Bind connection to the port number 7 (port of the Client). */
		err = netconn_bind(conn, IP_ADDR_ANY, 7);

		if (err == ERR_OK)
		{
			/* The desination IP adress of the computer */
			IP_ADDR4(&dest_addr, 192, 168,0, 2);
			dest_port = 10;  // server port

			/* Connect to the TCP Server
			while(netconn_connect(conn, &dest_addr, dest_port) != ERR_OK){
				IP_ADDR4(&dest_addr, 192, 168,0, 2);
							dest_port = 10;  // server port
			}
			*/
			connect_error = netconn_connect(conn, &dest_addr, dest_port);

			// If the connection to the server is established, the following will continue, else delete the connection
			if (connect_error == ERR_OK)
			{
				// Release the semaphore once the connection is successful
				sys_sem_signal(&tcpsem);
				while (1)
				{
					/* wait until the data is sent by the server */
					if (netconn_recv(conn, &buf) == ERR_OK)
					{
						/* Extract the address and port in case they are required */
						addr = netbuf_fromaddr(buf);  // get the address of the client
						port = netbuf_fromport(buf);  // get the Port of the client

						/* If there is some data remaining to be sent, the following process will continue */
						do
						{

							strncpy (msgc, buf->p->payload, buf->p->len);   // get the message from the server

							// Or modify the message received, so that we can send it back to the server
							sprintf (smsgc, "\"%s\" was sent by the Server\n", msgc);

							// semaphore must be taken before accessing the tcpsend function
							sys_arch_sem_wait(&tcpsem, 500);

							// send the data to the TCP Server
							tcpsend (smsgc);

							memset (msgc, '\0', 100);  // clear the buffer
						}
						while (netbuf_next(buf) >0);

						netbuf_delete(buf);
					}
				}
			}

			else
			{
				/* Close connection and discard connection identifier. */
				netconn_close(conn);
				netconn_delete(conn);
			}

		}
		else
		{
			// if the binding wasn't successful, delete the netconn connection
			netconn_delete(conn);
		}
	}
	osDelay(1000);
}

static void tcpsend_thread (void *arg)
{
	for (;;)
	{
		sprintf (smsgc, "index value = %d\n", indx++);
		// semaphore must be taken before accessing the tcpsend function
		sys_arch_sem_wait(&tcpsem, 500);
		// send the data to the server
		tcpsend(smsgc);
		osDelay(500);
	}
}


void tcpserver_init(void)
{
  //sys_thread_new("tcp_thread", tcp_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);
  //sys_sem_new(tcpsem, 0);
  sys_thread_new("tcp_thread", tcpinit_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);
  //sys_thread_new("tcpsend_thread", tcpsend_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);

}
