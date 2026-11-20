/*
	see copyright notice in sqrdbg.h
*/
#include <string.h>

#include <squirrel.h>
import std;
import ks.core.types;
import ks.platform;
import ks.simd;
#include "sqrdbg.h"
#include "sqdbgserver.h"
SQInteger debug_hook(HSQUIRRELVM v);
SQInteger error_handler(HSQUIRRELVM v);

#include "serialize_state.inl"
#if defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST)
#endif

HSQREMOTEDBG sq_rdbg_init(HSQUIRRELVM v,unsigned short port,SQBool autoupdate)
{
	ks::platform::address bindaddr{};
	SQDbgServer *rdbg = new SQDbgServer(v);
	rdbg->_autoupdate = autoupdate?true:false;
	rdbg->_accept = ks::platform::open_tcp(nullptr);
	bindaddr.port = port;
	bindaddr.ip = 0;
	if(!ks::platform::bind(rdbg->_accept,bindaddr,nullptr)){
		delete rdbg;
		sq_throwerror(v,_SC("failed to bind the socket"));
		return nullptr;
	}
	if(!rdbg->Init()) {
		delete rdbg;
		sq_throwerror(v,_SC("failed to initialize the debugger"));
		return nullptr;
	}
	
    return rdbg;
}

SQRESULT sq_rdbg_waitforconnections(HSQREMOTEDBG rdbg)
{
	if(SQ_FAILED(sq_compilebuffer(rdbg->_v,serialize_state_nut,(SQInteger)scstrlen(serialize_state_nut),_SC("SERIALIZE_STATE"),SQFalse))) {
		sq_throwerror(rdbg->_v,_SC("error compiling the serialization function"));
	}
	sq_getstackobj(rdbg->_v,-1,&rdbg->_serializefunc);
	sq_addref(rdbg->_v,&rdbg->_serializefunc);
	sq_pop(rdbg->_v,1);

	ks::platform::address cliaddr{};
	if(!ks::platform::listen_on(rdbg->_accept,0,nullptr))
		return sq_throwerror(rdbg->_v,_SC("error on listen(socket)"));
	rdbg->_endpoint = ks::platform::accept_connection(rdbg->_accept,&cliaddr,nullptr);
	//do not accept any other connection
	sqdbg_closesocket(rdbg->_accept);
	rdbg->_accept = nullptr;
	if(rdbg->_endpoint==nullptr){
		return sq_throwerror(rdbg->_v,_SC("error accept(socket)"));
	}
	while(!rdbg->_ready){
		sq_rdbg_update(rdbg);
	}
	return SQ_OK;
}

SQRESULT sq_rdbg_update(HSQREMOTEDBG rdbg)
{
	if(rdbg->_endpoint!=nullptr && ks::platform::wait_readable(rdbg->_endpoint,0,nullptr)){
		char temp[1024];
		int size=0;
		char c,prev=0;
		memset(&temp,0,sizeof(temp));
		int res;
		while((res = (int)ks::platform::receive_bytes(rdbg->_endpoint,&c,1,nullptr))>0){
			
			if(c=='\n')break;
			if(c!='\r'){
				temp[size]=c;
				prev=c;
				size++;
			}
		}
		switch(res){
		case 0:
			return sq_throwerror(rdbg->_v,_SC("disconnected"));
		case -1:
			return sq_throwerror(rdbg->_v,_SC("socket error"));
        }
		
		temp[size]=0;
		temp[size+1]=0;
		rdbg->ParseMsg(temp);
	}
	return SQ_OK;
}

SQInteger debug_hook(HSQUIRRELVM v)
{
	SQUserPointer up;
	intp event_type,line;
	const SQChar *src,*func;
	sq_getinteger(v,2,&event_type);
	sq_getstring(v,3,&src);
	sq_getinteger(v,4,&line);
	sq_getstring(v,5,&func);
	sq_getuserpointer(v,-1,&up);
	HSQREMOTEDBG rdbg = (HSQREMOTEDBG)up;
	rdbg->Hook(event_type,line,src,func);
	if(rdbg->_autoupdate) {
		if(SQ_FAILED(sq_rdbg_update(rdbg)))
			return sq_throwerror(v,_SC("socket failed"));
	}
	return 0;
}

SQInteger error_handler(HSQUIRRELVM v)
{
	SQUserPointer up;
	const SQChar *sErr=nullptr;
	const SQChar *fn=_SC("unknown");
	const SQChar *src=_SC("unknown");
	int line=-1;
	SQStackInfos si;
	sq_getuserpointer(v,-1,&up);
	HSQREMOTEDBG rdbg=(HSQREMOTEDBG)up;
	if(SQ_SUCCEEDED(sq_stackinfos(v,1,&si)))
	{
		if(si.funcname)fn=si.funcname;
		if(si.source)src=si.source;
		line=si.line;
		scprintf(_SC("*FUNCTION [%s] %s line [%d]\n"),fn,src,si.line);
	}
	if(sq_gettop(v)>=1){
		if(SQ_SUCCEEDED(sq_getstring(v,2,&sErr)))	{
			scprintf(_SC("\nAN ERROR HAS OCCURED [%s]\n"),sErr);
			rdbg->Break(si.line,src,_SC("error"),sErr);
		}
		else{
			scprintf(_SC("\nAN ERROR HAS OCCURED [unknown]\n"));
			rdbg->Break(si.line,src,_SC("error"),_SC("unknown"));
		}
	}
	rdbg->BreakExecution();
	return 0;
}


SQRESULT sq_rdbg_shutdown(HSQREMOTEDBG rdbg)
{
	delete rdbg;
	return SQ_OK;
}
