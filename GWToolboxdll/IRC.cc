/*
	cpIRC - C++ class based IRC protocol wrapper
	Copyright (C) 2003 Iain Sheppard

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	Contacting the author:
	~~~~~~~~~~~~~~~~~~~~~~

	email:	iainsheppard@yahoo.co.uk
	IRC:	#magpie @ irc.quakenet.org
*/
#include "stdafx.h"
#pragma warning(disable: 4100)
#pragma warning(disable: 4365)
#pragma warning(disable: 4456)
#pragma warning(disable: 4574)
#pragma warning(disable: 4625)
#pragma warning(disable: 4706)
#pragma warning(disable: 5039)

#include "IRC.h"
#ifdef WIN32
#include <windows.h>
#include <WS2tcpip.h>
#else
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#define closesocket(s) close(s)
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#endif


IRC::IRC()
{
	hooks=0;
	chan_users=0;
	connected=false;
	sentnick=false;
	sentpass=false;
	sentuser=false;
	pending_disconnect = false;
	cur_nick=0;
}
IRC::~IRC()
{
	if (hooks)
		delete_irc_command_hook(hooks);
    if (wsaData.wVersion)
        WSACleanup();
}

void IRC::insert_irc_command_hook(irc_command_hook* hook, const char* cmd_name, int (*function_ptr)(const char*, irc_reply_data*, void*))
{
	if (hook->function)
	{
		if (!hook->next)
		{
			hook->next=new irc_command_hook;
			hook->next->function=0;
			hook->next->irc_command=0;
			hook->next->next=0;
		}
		insert_irc_command_hook(hook->next, cmd_name, function_ptr);
	}
	else
	{
		hook->function=function_ptr;
		hook->irc_command=new char[strlen(cmd_name)+1];
		strcpy(hook->irc_command, cmd_name);
	}
}

void IRC::hook_irc_command(const char* cmd_name, int (*function_ptr)(const char*, irc_reply_data*, void*))
{
	if (!hooks)
	{
		hooks=new irc_command_hook;
		hooks->function=0;
		hooks->irc_command=0;
		hooks->next=0;
		insert_irc_command_hook(hooks, cmd_name, function_ptr);
	}
	else
	{
		insert_irc_command_hook(hooks, cmd_name, function_ptr);
	}
}

void IRC::delete_irc_command_hook(irc_command_hook* cmd_hook)
{
	if (cmd_hook->next)
		delete_irc_command_hook(cmd_hook->next);
	if (cmd_hook->irc_command)
		delete cmd_hook->irc_command;
	delete cmd_hook;
}

int IRC::start(const char* server, int port, const char* nick, const char* user, const char* name, const char* pass)
{
    struct addrinfo hints, * servinfo;
    if (connected) {
        printf("IRC::start called when already connected\n");
        return 1;
    }
	ping_sent = 0;
    //Ensure that servinfo is clear
    memset(&hints, 0, sizeof hints); //make sure the struct is empty

    //setup hints
    hints.ai_family = AF_UNSPEC;    //IPv4 or IPv6 doesnt matter
    hints.ai_socktype = SOCK_STREAM;        //TCP stream socket

    //Start WSA to be able to make DNS lookup
    int res;
    if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        printf("Failed to call WSAStartup: %d\n", res);
        return 1;
	}

    if ((res = getaddrinfo(server, "6667", &hints, &servinfo)) != 0) {
        printf("Failed to getaddrinfo: %s, %s\n", server, gai_strerror(res));
        return 1;
    }

    //setup socket
    if ((irc_socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == INVALID_SOCKET) {
        printf("Failed to socket: %d\n", WSAGetLastError());
        return 1;
    }
	disconnect();

    //Connect
    if (connect(irc_socket, servinfo->ai_addr, servinfo->ai_addrlen) == SOCKET_ERROR) {
        printf("Failed to connect: %d\n", WSAGetLastError());
        closesocket(irc_socket);
        return 1;
    }

    //We dont need this anymore
    freeaddrinfo(servinfo);

	connected=true;
	t = std::thread([&]() {
		message_loop();
		});
	t.detach();
    raw("PASS %s\r\n", pass);
    raw("USER %s\r\n", user);
    raw("NICK %s\r\n", user);
    
    return 0;
}
void IRC::disconnect()
{
	if (!connected) return;
	if(dataout) fclose(dataout);
	printf("Disconnected from server.\n");
	connected=false;
	quit("Leaving");
	#ifdef WIN32
	shutdown(irc_socket, 2);
	#endif
	closesocket(irc_socket);
	// Destroy thread.
	pending_disconnect = true;
	if (t.joinable())
		t.join();
	pending_disconnect = false;
	t.~thread();
}

void IRC::error(int err) {
    printf("Error: %d\n", err);
}
int IRC::quit(const char* quit_message)
{
    if (quit_message)
        return raw("QUIT %s\r\n", quit_message);
    return raw("QUIT\r\n");
}
int IRC::message_fetch() {
    if (!connected) return 1;
    message_buffer[0] = '\0';
    int ret_len = recv(irc_socket, message_buffer, 1023, 0);
    if (ret_len == SOCKET_ERROR) {
        printf("IRC::message_fetch recv failed, %d\n", WSAGetLastError());
        connected = false;
        closesocket(irc_socket);
        return 1;
    }
    if (!message_buffer[0]) {
        printf("IRC::message_fetch message empty; graceful close?\n");
        connected = false;
        closesocket(irc_socket);
        return 1;
    }
    message_buffer[ret_len] = '\0';
    split_to_replies(message_buffer);
    return 0;
}
int IRC::ping() {
	if (!connected) return 1;
	if (pong_recieved > ping_sent)
		ping_sent = 0;
	const clock_t timeout = 60 * CLOCKS_PER_SEC;
	clock_t now = clock();
	if (!ping_sent && (!pong_recieved || now - pong_recieved > timeout)) {
		raw("PING\r\n");
		ping_sent = now;
		return 0;
	}
	if (ping_sent && now - ping_sent > timeout) {
		printf("IRC::ping failed to get pong response after timeout; graceful close?\n");
		connected = false;
		ping_sent = 0;
		closesocket(irc_socket);
		return 1;
	}
	return 0;
}
bool IRC::is_connected() {
	return connected;
}
int IRC::message_loop()
{
	while (1) {
        if (!connected) continue;
		if (pending_disconnect) return 0;
        if (message_fetch() != 0)
            return 1;
	}
	return 0;
}

void IRC::split_to_replies(const char* data)
{
	const char* p;
	while (p=strstr(data, "\r\n"))
	{
        *const_cast<char*>(p) = '\0';
		parse_irc_reply(data);
		data=p+2;
	}
}

int IRC::is_op(const char* channel, const char* nick)
{
	channel_user* cup;

	cup=chan_users;
	
	while (cup)
	{
		if (!strcmp(cup->channel, channel) && !strcmp(cup->nick, nick))
		{
			return cup->flags&IRC_USER_OP;
		}
		cup=cup->next;
	}

	return 0;
}

int IRC::is_voice(const char* channel, const char* nick)
{
	channel_user* cup;

	cup=chan_users;
	
	while (cup)
	{
		if (!strcmp(cup->channel, channel) && !strcmp(cup->nick, nick))
		{
			return cup->flags&IRC_USER_VOICE;
		}
		cup=cup->next;
	}

	return 0;
}

void IRC::parse_irc_reply(const char* data)
{
	char* hostd;
	char* cmd;
	char* params;
	//char buffer[514];
	irc_reply_data hostd_tmp;
	channel_user* cup;
	char* p;
	char* chan_temp;

	hostd_tmp.target=0;

	printf("%s\n", data);

	if (data[0]==':')
	{
        hostd = const_cast<char*>(&data[1]);
		cmd=strchr(hostd, ' ');
		if (!cmd)
			return;
		*cmd='\0';
		cmd++;
		params=strchr(cmd, ' ');
		if (params)
		{
			*params='\0';
			params++;
		}
		hostd_tmp.nick=hostd;
		hostd_tmp.ident=strchr(hostd, '!');
		if (hostd_tmp.ident)
		{
			*hostd_tmp.ident='\0';
			hostd_tmp.ident++;
			hostd_tmp.host=strchr(hostd_tmp.ident, '@');
			if (hostd_tmp.host)
			{
				*hostd_tmp.host='\0';
				hostd_tmp.host++;
			}
		}

		if (!strcmp(cmd, "JOIN"))
		{
			cup=chan_users;
			if (cup)
			{
				while (cup->nick)
				{
					if (!cup->next)
					{
						cup->next=new channel_user;
						cup->next->channel=0;
						cup->next->flags=0;
						cup->next->next=0;
						cup->next->nick=0;
					}
					cup=cup->next;
				}
				cup->channel=new char[strlen(params)+1];
				strcpy(cup->channel, params);
				cup->nick=new char[strlen(hostd_tmp.nick)+1];
				strcpy(cup->nick, hostd_tmp.nick);
			}
		}
		else if (!strcmp(cmd, "PART"))
		{
			channel_user* d;
			channel_user* prev;

			d=0;
			prev=0;
			cup=chan_users;
			while (cup)
			{
				if (!strcmp(cup->channel, params) && !strcmp(cup->nick, hostd_tmp.nick))
				{
					d=cup;
					break;
				}
				else
				{
					prev=cup;
				}
				cup=cup->next;
			}
			if (d)
			{
				if (d==chan_users)
				{
					chan_users=d->next;
					if (d->channel)
						delete [] d->channel;
					if (d->nick)
						delete [] d->nick;
					delete d;
				}
				else
				{
					if (prev)
					{
						prev->next=d->next;
					}
					chan_users=d->next;
					if (d->channel)
						delete [] d->channel;
					if (d->nick)
						delete [] d->nick;
					delete d;
				}
			}
		}
		else if (!strcmp(cmd, "QUIT"))
		{
			channel_user* d;
			channel_user* prev;

			d=0;
			prev=0;
			cup=chan_users;
			while (cup)
			{
				if (!strcmp(cup->nick, hostd_tmp.nick))
				{
					d=cup;
					if (d==chan_users)
					{
						chan_users=d->next;
						if (d->channel)
							delete [] d->channel;
						if (d->nick)
							delete [] d->nick;
						delete d;
					}
					else
					{
						if (prev)
						{
							prev->next=d->next;
						}
						if (d->channel)
							delete [] d->channel;
						if (d->nick)
							delete [] d->nick;
						delete d;
					}
					break;
				}
				else
				{
					prev=cup;
				}
				cup=cup->next;
			}
		}
		else if (!strcmp(cmd, "MODE"))
		{
			char* chan;
			char* changevars;
			channel_user* cup;
			channel_user* d;
			char* tmp;
			int i;
			bool plus;

			chan=params;
			params=strchr(chan, ' ');
			*params='\0';
			params++;
			changevars=params;
			params=strchr(changevars, ' ');
			if (!params)
			{
				return;
			}
			if (chan[0]!='#')
			{
				return;
			}
			*params='\0';
			params++;
		
			plus=false;
			for (i=0; i<(signed)strlen(changevars); i++)
			{
				switch (changevars[i])
				{
				case '+':
					plus=true;
					break;
				case '-':
					plus=false;
					break;
				case 'o':
					tmp=strchr(params, ' ');
					if (tmp)
					{
						*tmp='\0';
						tmp++;
					}
					tmp=params;
					if (plus)
					{
						// user has been opped (chan, params)
						cup=chan_users;
						d=0;
						while (cup)
						{
							if (cup->next && cup->channel)
							{
								if (!strcmp(cup->channel, chan) && !strcmp(cup->nick, tmp))
								{
									d=cup;
									break;
								}
							}
							cup=cup->next;
						}
						if (d)
						{
							d->flags=d->flags|IRC_USER_OP;
						}
					}
					else
					{
						// user has been deopped (chan, params)
						cup=chan_users;
						d=0;
						while (cup)
						{
							if (!strcmp(cup->channel, chan) && !strcmp(cup->nick, tmp))
							{
								d=cup;
								break;
							}
							cup=cup->next;
						}
						if (d)
						{
							d->flags=d->flags^IRC_USER_OP;
						}
					}
					params=tmp;
					break;
				case 'v':
					tmp=strchr(params, ' ');
					if (tmp)
					{
						*tmp='\0';
						tmp++;
					}
					if (plus)
					{
						// user has been voiced
						cup=chan_users;
						d=0;
						while (cup)
						{
							if (!strcmp(cup->channel, params) && !strcmp(cup->nick, hostd_tmp.nick))
							{
								d=cup;
								break;
							}
							cup=cup->next;
						}
						if (d)
						{
							d->flags=d->flags|IRC_USER_VOICE;
						}
					}
					else
					{
						// user has been devoiced
						cup=chan_users;
						d=0;
						while (cup)
						{
							if (!strcmp(cup->channel, params) && !strcmp(cup->nick, hostd_tmp.nick))
							{
								d=cup;
								break;
							}
							cup=cup->next;
						}
						if (d)
						{
							d->flags=d->flags^IRC_USER_VOICE;
						}
					}
					params=tmp;
					break;
				default:
					return;
					break;
				}
				// ------------ END OF MODE ---------------
			}
		}
		else if (!strcmp(cmd, "353"))
		{
			// receiving channel names list
			if (!chan_users)
			{
				chan_users=new channel_user;
				chan_users->next=0;
				chan_users->nick=0;
				chan_users->flags=0;
				chan_users->channel=0;
			}
			cup=chan_users;
			chan_temp=strchr(params, '#');
			if (chan_temp)
			{
				//chan_temp+=3;
				p=strstr(chan_temp, " :");
				if (p)
				{
					*p='\0';
					p+=2;
					while (strchr(p, ' '))
					{
						char* tmp;

						tmp=strchr(p, ' ');
						*tmp='\0';
						tmp++;
						while (cup->nick)
						{
							if (!cup->next)
							{
								cup->next=new channel_user;
								cup->next->channel=0;
								cup->next->flags=0;
								cup->next->next=0;
								cup->next->nick=0;
							}
							cup=cup->next;
						}
						if (p[0]=='@')
						{
							cup->flags=cup->flags|IRC_USER_OP;
							p++;
						}
						else if (p[0]=='+')
						{
							cup->flags=cup->flags|IRC_USER_VOICE;
							p++;
						}
						cup->nick=new char[strlen(p)+1];
						strcpy(cup->nick, p);
						cup->channel=new char[strlen(chan_temp)+1];
						strcpy(cup->channel, chan_temp);
						p=tmp;
					}
					while (cup->nick)
					{
						if (!cup->next)
						{
							cup->next=new channel_user;
							cup->next->channel=0;
							cup->next->flags=0;
							cup->next->next=0;
							cup->next->nick=0;
						}
						cup=cup->next;
					}
					if (p[0]=='@')
					{
						cup->flags=cup->flags|IRC_USER_OP;
						p++;
					}
					else if (p[0]=='+')
					{
						cup->flags=cup->flags|IRC_USER_VOICE;
						p++;
					}
					cup->nick=new char[strlen(p)+1];
					strcpy(cup->nick, p);
					cup->channel=new char[strlen(chan_temp)+1];
					strcpy(cup->channel, chan_temp);
				}
			}
		}
		else if (!strcmp(cmd, "NOTICE"))
		{
			hostd_tmp.target=params;
			params=strchr(hostd_tmp.target, ' ');
			if (params)
				*params='\0';
			params++;
			#ifdef __IRC_DEBUG__
			printf("%s >-%s- %s\n", hostd_tmp.nick, hostd_tmp.target, &params[1]);
			#endif
		}
		else if (!strcmp(cmd, "PRIVMSG"))
		{
			hostd_tmp.target=params;
			params=strchr(hostd_tmp.target, ' ');
			if (!params)
				return;
			*(params++)='\0';
			#ifdef __IRC_DEBUG__
			printf("%s: <%s> %s\n", hostd_tmp.target, hostd_tmp.nick, &params[1]);
			#endif
		}
		else if (!strcmp(cmd, "NICK"))
		{
			if (!strcmp(hostd_tmp.nick, cur_nick))
			{
				delete [] cur_nick;
				cur_nick=new char[strlen(params)+1];
				strcpy(cur_nick, params);
			}
		}
		/* else if (!strcmp(cmd, ""))
		{
			#ifdef __IRC_DEBUG__
			#endif
		} */
		call_hook(cmd, params, &hostd_tmp);
	}
	else
	{
		cmd=const_cast<char*>(data);
		data=strchr(cmd, ' ');
		if (!data)
			return;
        *const_cast<char*>(data) = '\0';
        params=const_cast<char*>(data+1);

		if (!strcmp(cmd, "PING"))
		{
			if (!params)
				return;
            raw("PONG %s\r\n", &params[1]);
			#ifdef __IRC_DEBUG__
			printf("Ping received, pong sent.\n");
			#endif
			fflush(dataout);
		}
		else if (!strcmp(cmd, "PONG"))
		{
			pong_recieved = clock();
			fflush(dataout);
		}
		else
		{
			hostd_tmp.host=0;
			hostd_tmp.ident=0;
			hostd_tmp.nick=0;
			hostd_tmp.target=0;
			call_hook(cmd, params, &hostd_tmp);
		}
	}
}

void IRC::call_hook(const char* irc_command, const char* params, irc_reply_data* hostd)
{
	irc_command_hook* p;

	if (!hooks)
		return;

	p=hooks;
	while (p)
	{
		if (!strcmp(p->irc_command, irc_command))
		{
			(*(p->function))(params, hostd, this);
			p=0;
		}
		else
		{
			p=p->next;
		}
	}
}

int IRC::notice(const char* target, const char* message)
{
	if (!connected)
		return 1;
	fprintf(dataout, "NOTICE %s :%s\r\n", target, message);
	return fflush(dataout);
}

int IRC::notice(const char* fmt, ...)
{
	va_list argp;
	//char* target;
	
	if (!connected)
		return 1;
	va_start(argp, fmt);
	fprintf(dataout, "NOTICE %s :", fmt);
	vfprintf(dataout, va_arg(argp, char*), argp);
	va_end(argp);
	fprintf(dataout, "\r\n");
	return fflush(dataout);
}

int IRC::privmsg(const char* target, const char* message)
{
    return raw("PRIVMSG %s :%s\r\n", target, message);
}

int IRC::privmsg(const char* fmt, ...)
{
	if (!connected)
		return 1;
    va_list args;
	va_start(args, fmt);
	raw("PRIVMSG %s :", fmt);
    int ok = raw(va_arg(args, char*), args);
	va_end(args);
    return ok;
}

int IRC::part(const char* channel)
{
    return raw("PART %s\r\n", channel);
}

int IRC::raw(const wchar_t* fmt, ...) {
    wchar_t buffer[600];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(buffer, 599, fmt, args);
    va_end(args);
    return raw("%ls", buffer);
}

int IRC::raw(const char* fmt, ...) {
    if (!connected) {
        printf("IRC:raw called when not connected\n");
        return 1;
    }
    char buffer[600];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, 599, fmt, args) + 1;
    va_end(args);
    if (len < 1) {
        printf("IRC::raw, no message bytes or failure\n");
        return 1;
    }
    // Add new line at the end if missing
    if (len < 3 || buffer[len - 3] != '\r\n') {
        buffer[len] = '\r';
        buffer[len + 1] = '\n';
        buffer[len + 2] = '\0';
    }
    printf("IRC::raw sending %s\n", buffer);
    int sent_bytes = send(irc_socket, buffer, (int)strlen(buffer), NULL);
    if (sent_bytes == SOCKET_ERROR) {
        printf("IRC::raw send() failed, %d\n", WSAGetLastError());
        return 1;
    }
    printf("IRC::raw send() %d bytes sent\n", sent_bytes);
    return 0;
}

int IRC::kick(const char* channel, const char* nick, const char* message)
{
    return raw("KICK %s %s :%s\r\n", channel, nick, message);
}

int IRC::mode(const char* channel, const char* modes, const char* targets)
{
    if(!targets)
        return raw("MODE %s %s\r\n", channel, modes);
    return raw("MODE %s %s %s\r\n", channel, modes, targets);
}

int IRC::mode(const char* modes)
{
    return mode(cur_nick, modes, 0);
}

int IRC::nick(const char* newnick)
{
    return raw("NICK %s\r\n", newnick);
}

char* IRC::current_nick()
{
	return cur_nick;
}
