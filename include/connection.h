/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "base.h"
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <vector>
#include <deque>
#include <sstream>

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#define STATE_DISCONNECTED	0
#define STATE_CONNECTED		1
#define STATE_SYNC		2
#define STATE_NOAUTH_INBOUND	3
#define STATE_NOAUTH_OUTBOUND	4
#define STATE_SERVICES		5

std::string CreateSum();

/** Each connection has one or more of these
 * each represents ONE outbound connection to another ircd
 * so each inbound has multiple outbounds. A listening socket
 * that accepts server type connections is represented by one
 * class serverrec. Class serverrec will instantiate several
 * objects of type ircd_connector to represent each established
 * connection, inbound or outbound. So, to determine all linked
 * servers you must walk through all the serverrecs that the
 * core defines, and in each one iterate through until you find
 * connection(s) relating to the server you want information on.
 * The core and module API provide functions for this.
 */
class ircd_connector : public Extensible
{
 private:
	/** Sockaddr of the outbound ip and port
	 */
	sockaddr_in addr;
	
	/** File descriptor of the connection
	 */
	int fd;
	
	/** Server name
	 */
	std::string servername;
	
	/** Server 'GECOS'
	 */
	std::string description;
	
	/** State. STATE_NOAUTH_INBOUND, STATE_NOAUTH_OUTBOUND
	 * STATE_SYNC, STATE_DISCONNECTED, STATE_CONNECTED
	 */
	char state;
	
	/** PRIVATE function to set the host address and port to connect to
	 */
	bool SetHostAddress(char* host, int port);

	/** This string holds the ircd's version response
	 */
	std::string version;

 public:

        /** IRCD Buffer for input characters, holds as many lines as are
	 * pending - Note that the final line may not be complete and should
	 * only be read when there is a \n seperator.
         */
        std::string ircdbuffer;

 
	/** When MakeOutboundConnection is called, these public members are
 	 * filled with the details passed to the function, for future
 	 * reference
 	 */
	char host[MAXBUF];

	/** When MakeOutboundConnection is called, these public members are
 	 * filled with the details passed to the function, for future
 	 * reference
 	 */
	int port;
	
	/** Server names of servers that this server is linked to
	 * So for A->B->C, if this was the record for B it would contain A and C
	 * whilever both servers are connected to B.
	 */
	std::vector<std::string> routes;
	

	/** Create an outbound connection to a listening socket
	 */ 
	bool MakeOutboundConnection(char* newhost, int newport);
	
	/** Return the servername on this established connection
	 */
	std::string GetServerName();
	
	/** Set the server name of this connection
	 */
	void SetServerName(std::string serv);
	
	/** Get the file descriptor associated with this connection
	 */
	int GetDescriptor();
	
	/** Set the file descriptor for this connection
	 */
	void SetDescriptor(int fd);
	
	/** Get the state flags for this connection
	 */
	int GetState();
	
	/** Set the state flags for this connection
	 */
	void SetState(int state);
	
	/** Get the ip address (not servername) associated with this connection
	 */
	char* GetServerIP();
	
	/** Get the server description of this connection
	 */
	std::string GetDescription();
	
	/** Set the server description of this connection
	 */
	void SetDescription(std::string desc);
	
	/** Get the port number being used for this connection
	 * If the connection is outbound this will be the remote port
	 * otherwise it will be the local port, so it can always be
	 * gautanteed as open at the address given in GetServerIP().
	 */
	int GetServerPort();
	
	/** Set the port used by this connection
	 */
	void SetServerPort(int p);
	
	/** Set both the host and the port in one operation for this connection
	 */
	bool SetHostAndPort(char* newhost, int newport);
	
	/** Close the connection by calling close() on its file descriptor
	 * This function call updates no other data.
	 */
	void CloseConnection();

	/** This method adds text to the ircd connection's buffer
	 * There is no limitation on how much text of what line width may
	 * be added to this buffer. It is the sending server's responsibility
	 * to ensure sent data is kept within reasonable quanities.
	 */
	void AddBuffer(std::string a);

	/** This method returns true if the buffer contains at least one
	 * carriage return character, e.g. one line can be read from the
	 * buffer successfully.
	 */
	bool BufferIsComplete();

	/** This method clears the server's buffer by setting it to an empty string.
	 */
	void ClearBuffer();

	/** This method retrieves the first string from the tail end of the
	 * buffer and advances the tail end of the buffer past the returned
	 * string, in a similar manner to strtok().
	 */
	std::string GetBuffer();

	/** This method sets the version string of the remote server
	 */
        void SetVersionString(std::string newversion);

	/** This method returns the version string of the remote server.
	 * If the server has no version string an empty string is returned.
	 */
        std::string GetVersionString();
};


/** Please note: classes serverrec and userrec both inherit from class connection.
 */
class connection : public Extensible
{
 public:
	/** File descriptor of the connection
	 */
	int fd;
	
	/** Hostname of connection. Not used if this is a serverrec
	 */
	char host[160];
	
	/** IP of connection.
	 */
	char ip[16];
	
	/** Stats counter for bytes inbound
	 */
	int bytes_in;

	/** Stats counter for bytes outbound
	 */
	int bytes_out;

	/** Stats counter for commands inbound
	 */
	int cmds_in;

	/** Stats counter for commands outbound
	 */
	int cmds_out;

	/** True if server/user has authenticated, false if otherwise
	 */
	bool haspassed;

	/** Port number
	 * For a userrec, this is the port they connected to the network on.
	 * For a serverrec this is the current listening port of the serverrec object.
	 */
	int port;
	
	/** Used by userrec to indicate the registration status of the connection
	 */
	char registered;
	
	/** Time the connection was last pinged
	 */
	time_t lastping;
	
	/** Time the connection was created, set in the constructor
	 */
	time_t signon;
	
	/** Time that the connection last sent data, used to calculate idle time
	 */
	time_t idle_lastmsg;
	
	/** Used by PING checks with clients
	 */
	time_t nping;
	
	/** Default constructor
	 */
	connection();
};


#endif
