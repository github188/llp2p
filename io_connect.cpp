
#include "io_connect.h"
#include "pk_mgr.h"
#include "network.h"
#include "logger.h"
#include "peer_mgr.h"
#include "peer.h"
#include "peer_communication.h"
#include "logger_client.h"
#include "io_nonblocking.h"

using namespace std;

io_connect::io_connect(network *net_ptr,logger *log_ptr,configuration *prep_ptr,peer_mgr * peer_mgr_ptr,peer *peer_ptr,pk_mgr * pk_mgr_ptr, peer_communication *peer_communication_ptr, logger_client * logger_client_ptr)
{
	_net_ptr = net_ptr;
	_log_ptr = log_ptr;
	_prep = prep_ptr;
	_peer_mgr_ptr = peer_mgr_ptr;
	_peer_ptr = peer_ptr;
	_pk_mgr_ptr = pk_mgr_ptr;
	_peer_communication_ptr = peer_communication_ptr;
	_logger_client_ptr = logger_client_ptr;
}

io_connect::~io_connect()
{
	debug_printf("Have deleted io_connect \n");
}

int io_connect::handle_pkt_in(int sock)
{	
	/*
	we will not inside this part
	*/
	
	int error_return;
	int error_num,error_len;

	error_len = sizeof(error_num);
	error_num = -1;
	error_return = getsockopt(sock,SOL_SOCKET, SO_ERROR, (char*)&error_num, (socklen_t *)&error_len);
	
	_log_ptr->write_log_format("s(u) s d s d (d) \n", __FUNCTION__, __LINE__, "[ERROR] sock", sock, "call getsockopt error num :", error_return, error_num);
	
	if (error_return != 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d (d) (d) \n", "[ERROR] call getsockopt error num :", error_return, socketErr, error_num);
		debug_printf("[ERROR] call getsockopt error num : %d (%d) (%d) \n", error_return, socketErr, error_num);
		_log_ptr->write_log_format("s(u) s d (d) (d) \n", __FUNCTION__, __LINE__, "[ERROR] call getsockopt error num :", error_return, socketErr, error_num);
		_logger_client_ptr->log_exit();
		PAUSE
	}
	
	
	
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s \n","error : place in io_connect::handle_pkt_in\n");
	_logger_client_ptr->log_exit();

	PAUSE
	
	return RET_OK;
}

int io_connect::handle_pkt_out(int sock)
{
	/*
	in this part means the fd is built. it finds its role and sends protocol to another to tell its role.
	bind to peer_com~ for handle_pkt_in/out.
	*/
	
	//_net_ptr->set_nonblocking(sock);
	int error_return;
	int error_num;
	int error_len;

	// Check the connection is successful or not
	error_len = sizeof(error_num);
	error_num = -1;
	error_return = getsockopt(sock,SOL_SOCKET, SO_ERROR, (char*)&error_num, (socklen_t *)&error_len);
	
	_log_ptr->write_log_format("s(u) s d s d (d) \n", __FUNCTION__, __LINE__, "[ERROR] sock", sock, "call getsockopt error num :", error_return, error_num);
	
	if (error_return != 0) {
#ifdef _WIN32
		int socketErr = WSAGetLastError();
#else
		int socketErr = errno;
#endif
		_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s d (d) (d) \n", "[ERROR] call getsockopt error num :", error_return, socketErr, error_num);
		debug_printf("[ERROR] call getsockopt error num : %d (%d) (%d) \n", error_return, socketErr, error_num);
		_log_ptr->write_log_format("s(u) s d (d) (d) \n", __FUNCTION__, __LINE__, "[ERROR] call getsockopt error num :", error_return, socketErr, error_num);
		_logger_client_ptr->log_exit();
		PAUSE
	}
	else {
		// The connection is built. Next, we should determine the stream direction
		
		struct role_struct *role_protocol_ptr = NULL;
		Nonblocking_Ctl *Nonblocking_Send_Ctrl_ptr = NULL;
		int _send_byte = 0;

		map<int, struct ioNonBlocking*>::iterator map_fd_NonBlockIO_iter;
		map_fd_NonBlockIO_iter = _peer_communication_ptr->map_fd_NonBlockIO.find(sock);
		if (map_fd_NonBlockIO_iter == _peer_communication_ptr->map_fd_NonBlockIO.end()) {
			_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] not found _peer_communication_ptr->map_fd_NonBlockIO \n");
			debug_printf("[ERROR] not found _peer_communication_ptr->map_fd_NonBlockIO \n");
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] not found _peer_communication_ptr->map_fd_NonBlockIO");
			*(_net_ptr->_errorRestartFlag) = RESTART;
			PAUSE
		}

		Nonblocking_Send_Ctrl_ptr = &(map_fd_NonBlockIO_iter->second->io_nonblockBuff.nonBlockingSendCtrl);
		
		if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {

			map<int, int>::iterator map_fd_flag_iter;
			map<int, unsigned long>::iterator map_fd_session_id_iter;		//must be store before connect
			map<int, unsigned long>::iterator map_peer_com_fd_pid_iter;		//must be store before connect
			map<unsigned long, struct peer_com_info *>::iterator session_id_candidates_set_iter;
			map<int, struct fd_information *>::iterator map_fd_info_iter;
			int send_byte;

			role_protocol_ptr = new struct role_struct;
			if (!role_protocol_ptr) {
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] role_protocol_ptr new error \n");
				debug_printf("[ERROR] role_protocol_ptr new error \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] role_protocol_ptr new error");
				PAUSE
			}
			memset(role_protocol_ptr, 0, sizeof(struct role_struct));

			role_protocol_ptr->header.cmd = CHNK_CMD_ROLE;
			role_protocol_ptr->header.length = sizeof(struct role_struct) - sizeof(struct chunk_header_t);
			role_protocol_ptr->header.rsv_1 = REQUEST;

			map_fd_info_iter = _peer_communication_ptr->map_fd_info.find(sock);
			if (map_fd_info_iter == _peer_communication_ptr->map_fd_info.end()) {
				_logger_client_ptr->log_to_server(LOG_WRITE_STRING, 0, "s \n", "[ERROR] not found _peer_communication_ptr->map_fd_info \n");
				debug_printf("[ERROR] not found _peer_communication_ptr->map_fd_info \n");
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "[ERROR] not found _peer_communication_ptr->map_fd_info");
				_logger_client_ptr->log_exit();
			}

			// Tell that peer what my role is
			if (map_fd_info_iter->second->role == CHILD_PEER) {
				role_protocol_ptr->flag = PARENT_PEER;
			}
			else {
				role_protocol_ptr->flag = CHILD_PEER;
			}

			role_protocol_ptr->manifest = map_fd_info_iter->second->manifest;
			role_protocol_ptr->recv_pid = map_fd_info_iter->second->pid;
			role_protocol_ptr->send_pid = _peer_mgr_ptr->self_pid;
		

			//_net_ptr->set_blocking(sock);
			//int send_offset,expect_len;
			//expect_len = sizeof(struct role_struct);
			//send_offset = 0;

			//while(1){
			//	send_byte = _net_ptr->send(sock, (char*)role_protocol_ptr + send_offset, expect_len, 0);
			//	if(send_byte<0){
			//		
			//		_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error : send CHNK_CMD_ROLE number :",WSAGetLastError());
			//		_logger_client_ptr->log_exit();
			//	}
			//	else{
			//		expect_len = expect_len - send_byte;
			//		send_offset = send_offset + send_byte;
			//		if(expect_len == 0){
			//			send_offset = 0;
			//			delete role_protocol_ptr;
			//			break;
			//		}
			//	}
			//}

			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.offset = 0;
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.total_len = sizeof(struct role_struct);
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.expect_len = sizeof(struct role_struct);
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.buffer = (char *)role_protocol_ptr;
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = (chunk_t *)role_protocol_ptr;
			Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.serial_num =  role_protocol_ptr->header.sequence_number;

			_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));

			if (_send_byte < 0) {
				data_close(sock, "io connect error \n");
				return RET_SOCK_ERROR;
			} 
			else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == READY) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY first send OK");
				_net_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
				//_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr->_peer_ptr));
				if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
					delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
				}
				Nonblocking_Send_Ctrl_ptr->recv_ctl_info.chunk_ptr = NULL;
			}
		}
		else if (Nonblocking_Send_Ctrl_ptr->recv_ctl_info.ctl_state == RUNNING) {
			_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING state send ");

			_send_byte = _net_ptr->nonblock_send(sock, &(Nonblocking_Send_Ctrl_ptr->recv_ctl_info));

			PAUSE
			
			//if(!(_logger_client_ptr->log_bw_out_init_flag)){
			//	_logger_client_ptr->log_bw_out_init_flag = 1;
			//	_logger_client_ptr->bw_out_struct_init(_send_byte);
			//}
			//else{
			//	_logger_client_ptr->set_out_bw(_send_byte);
			//}

			if (_send_byte < 0) {
				data_close(sock, "io connect error\n");
				return RET_SOCK_ERROR;
			}
			else if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == READY) {
				_log_ptr->write_log_format("s(u) s \n", __FUNCTION__, __LINE__, "Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.ctl_state == RUNNING  send OK");

				_net_ptr->epoll_control(sock, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT);
				_net_ptr->set_fd_bcptr_map(sock, dynamic_cast<basic_class *> (_peer_communication_ptr));
				
				if (Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr) {
					delete Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr;
				}
				Nonblocking_Send_Ctrl_ptr ->recv_ctl_info.chunk_ptr = NULL;
			}
		}
	}

	return RET_OK;
}

void io_connect::handle_pkt_error(int sock)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect handle_pkt_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}

void io_connect::handle_job_realtime()
{

}

void io_connect::handle_job_timer()
{

}

void io_connect::handle_sock_error(int sock, basic_class *bcptr)
{
#ifdef _WIN32
	int socketErr = WSAGetLastError();
#else
	int socketErr = errno;
#endif
	_peer_communication_ptr->fd_close(sock);
	_logger_client_ptr->log_to_server(LOG_WRITE_STRING,0,"s d \n","error in io_connect handle_sock_error error number : ",socketErr);
	_logger_client_ptr->log_exit();
}


void io_connect::data_close(int cfd, const char *reason) 
{

//	list<int>::iterator fd_iter;
//
//	_log_ptr->write_log_format("s => s (s)\n", (char*)__PRETTY_FUNCTION__, "pk", reason);
	cout << "pk Client " << cfd << " exit by " << reason << ".." << endl;
//	_net_ptr->close(cfd);
//
//	for(fd_iter = fd_list_ptr->begin(); fd_iter != fd_list_ptr->end(); fd_iter++) {
//		if(*fd_iter == cfd) {
//			fd_list_ptr->erase(fd_iter);
//			break;
//		}
//	}
////	PAUSE
	_peer_communication_ptr->fd_close(cfd);

}