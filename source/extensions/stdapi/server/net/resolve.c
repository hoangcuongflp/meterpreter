#include "precomp.h"
#include <stdio.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
#endif

DWORD resolve_host(LPCSTR hostname, u_short ai_family, struct in_addr *result)
{
	struct addrinfo hints, *list;
	struct in_addr addr;
	struct sockaddr_in *sockaddr_ipv4;
	struct sockaddr_in6 *sockaddr_ipv6;
	int iResult;
	
#ifdef _WIN32
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != NO_ERROR)
	{
		dprintf("Could not initialise Winsock: %x.", iResult);
		return iResult;
	}
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = 0;
	hints.ai_family = ai_family;

	dprintf("Attempting to resolve '%s'", hostname);
	
	iResult = getaddrinfo(hostname, NULL, &hints, &list);

	if (iResult != NO_ERROR)
	{
		dprintf("Unable to resolve host Error: %x.", iResult);
#ifndef _WIN32
		dprintf("Error msg: %s", gai_strerror(iResult));
#endif
	}
	else
	{
		switch (list->ai_family) {
		case AF_INET:
			sockaddr_ipv4 = (struct sockaddr_in *) list->ai_addr;
			addr = sockaddr_ipv4->sin_addr;
			memcpy((void*)result, &addr, sizeof(addr));
		case AF_INET6:
			//todo
		default:
			break;
		}
	}

#ifdef _WIN32
	// Causes segfaul in nix?
	freeaddrinfo(list);
	WSACleanup();
#endif

	return iResult;
}

DWORD request_resolve_host(Remote *remote, Packet *packet)
{
	Packet *response = packet_create_response(packet);
	LPCSTR hostname = NULL;
	struct in_addr addr;
	u_short ai_family = AF_INET;
	int iResult;

	hostname = packet_get_tlv_value_string(packet, TLV_TYPE_HOST_NAME);

	if (!hostname)
	{
		iResult = ERROR_INVALID_PARAMETER;
		dprintf("Hostname not set");
	}
	else
	{
		iResult = resolve_host(hostname, ai_family, &addr);
		if (iResult == NO_ERROR)
		{
#ifdef _WIN32
                        packet_add_tlv_raw(response, TLV_TYPE_IP, &addr, sizeof(addr));
#else
                        packet_add_tlv_raw(response, TLV_TYPE_IP, &(addr.s_addr), sizeof(addr.s_addr));
#endif
			packet_add_tlv_uint(response, TLV_TYPE_ADDR_TYPE, ai_family);
		}
		else
		{
			dprintf("Unable to resolve_host %s error: %x", hostname, iResult);
		}
	}

	packet_transmit_response(iResult, remote, response);
	return ERROR_SUCCESS;
}

DWORD request_resolve_hosts(Remote *remote, Packet *packet)
{
	Packet *response = packet_create_response(packet);
	Tlv hostname = {0};
	int index = 0;
	int iResult;

	while( packet_enum_tlv( packet, index++, TLV_TYPE_HOST_NAME, &hostname ) == ERROR_SUCCESS )
	{
		struct in_addr addr = {0};
		u_short ai_family = AF_INET;

		iResult = resolve_host((LPCSTR)hostname.buffer, ai_family, &addr);

		if (iResult == NO_ERROR)
		{
#ifdef _WIN32
			packet_add_tlv_raw(response, TLV_TYPE_IP, &addr, sizeof(struct in_addr));
#else
			packet_add_tlv_raw(response, TLV_TYPE_IP, &(addr.s_addr), sizeof(addr.s_addr));
#endif
		}
		else
		{
			dprintf("Unable to resolve_host %s error: %x", hostname.buffer, iResult);		
			packet_add_tlv_raw(response, TLV_TYPE_IP, NULL, 0);
		}
		packet_add_tlv_uint(response, TLV_TYPE_ADDR_TYPE, ai_family);
	}

	packet_transmit_response(NO_ERROR, remote, response);
	return ERROR_SUCCESS;
}
