// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

module;

#include <ctype.h>
#include <iconv.h>
#include <dirent.h>
#include <malloc.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/wait.h>
#include <pwd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>



module ks.platform;

struct ks::platform::mapping
{
	void *base;
	unsigned long long size;
};

struct ks::platform::library
{
	void *handle;
};

struct ks::platform::file
{
	int fd;
};

struct ks::platform::virtual_memory
{
	void *base;
	unsigned long long size;
};

namespace
{

ks::platform::file_error map_errno( int value ) noexcept
{
	switch ( value )
	{
	case ENOENT:
	case ENOTDIR:
		return ks::platform::file_error::not_found;
	case EACCES:
	case EPERM:
		return ks::platform::file_error::denied;
	default:
		return ks::platform::file_error::io;
	}
}

} // namespace

unsigned long long ks::platform::process_id() noexcept
{
	return (unsigned long long)getpid();
}

ks::platform::file *ks::platform::open_file( const char *path, file_mode mode, file_error *error ) noexcept
{
	file_error local = file_error::none;
	int flags = O_RDONLY;
	if ( mode == file_mode::write )
		flags = O_WRONLY | O_CREAT | O_TRUNC;
	else if ( mode == file_mode::write_keep )
		flags = O_WRONLY | O_CREAT;
	else if ( mode == file_mode::read_write )
		flags = O_RDWR | O_CREAT;
	int fd = ( path != nullptr ) ? ::open( path, flags, 0644 ) : -1;
	if ( fd < 0 )
		local = map_errno( errno );
	if ( error != nullptr )
		*error = local;
	return ( fd >= 0 ) ? new file{ fd } : nullptr;
}

long ks::platform::read_file( file *f, void *buffer, unsigned long size, file_error *error ) noexcept
{
	if ( f == nullptr || buffer == nullptr )
	{
		if ( error != nullptr )
			*error = file_error::io;
		return -1;
	}
	for ( ;; )
	{
		ssize_t got = ::read( f->fd, buffer, size );
		if ( got >= 0 )
		{
			if ( error != nullptr )
				*error = file_error::none;
			return (long)got;
		}
		if ( errno != EINTR )
		{
			if ( error != nullptr )
				*error = map_errno( errno );
			return -1;
		}
	}
}

long ks::platform::write_file( file *f, const void *data, unsigned long size, file_error *error ) noexcept
{
	if ( f == nullptr || data == nullptr )
	{
		if ( error != nullptr )
			*error = file_error::io;
		return -1;
	}
	unsigned long total = 0;
	const char *bytes = static_cast<const char *>( data );
	while ( total < size )
	{
		ssize_t written = ::write( f->fd, bytes + total, size - total );
		if ( written < 0 )
		{
			if ( errno == EINTR )
				continue;
			if ( error != nullptr )
				*error = map_errno( errno );
			return -1;
		}
		total += (unsigned long)written;
	}
	if ( error != nullptr )
		*error = file_error::none;
	return (long)total;
}

long long ks::platform::seek_file( file *f, long long offset, int origin, file_error *error ) noexcept
{
	if ( f == nullptr )
	{
		if ( error != nullptr )
			*error = file_error::io;
		return -1;
	}
	off_t result = ::lseek( f->fd, (off_t)offset, origin );
	if ( result < 0 )
	{
		if ( error != nullptr )
			*error = map_errno( errno );
		return -1;
	}
	if ( error != nullptr )
		*error = file_error::none;
	return (long long)result;
}

unsigned long long ks::platform::file_size( file *f ) noexcept
{
	struct stat st;
	if ( f == nullptr || ::fstat( f->fd, &st ) != 0 )
		return 0;
	return (unsigned long long)st.st_size;
}

bool ks::platform::set_file_times( file *f, long long access_seconds, long long modified_seconds ) noexcept
{
	if ( f == nullptr )
		return false;
	struct timeval times[2];
	times[0].tv_sec = (time_t)access_seconds;
	times[0].tv_usec = 0;
	times[1].tv_sec = (time_t)modified_seconds;
	times[1].tv_usec = 0;
	return ::futimes( f->fd, times ) == 0;
}

ks::platform::file *ks::platform::duplicate_file( file *f ) noexcept
{
	if ( f == nullptr )
		return nullptr;
	int copy = ::dup( f->fd );
	return ( copy >= 0 ) ? new file{ copy } : nullptr;
}

bool ks::platform::set_file_mode( file *f, unsigned int mode ) noexcept
{
	return f != nullptr && ::fchmod( f->fd, (mode_t)mode ) == 0;
}

bool ks::platform::lock_file( file *f, bool exclusive ) noexcept
{
	if ( f == nullptr )
		return false;
	struct flock lock;
	lock.l_type = exclusive ? F_WRLCK : F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	return ::fcntl( f->fd, F_SETLK, &lock ) != -1;
}

void ks::platform::close_file( file *f ) noexcept
{
	if ( f != nullptr )
	{
		::close( f->fd );
		delete f;
	}
}

int ks::platform::last_errno() noexcept
{
	return errno;
}

void ks::platform::clear_errno() noexcept
{
	errno = 0;
}

unsigned long long ks::platform::monotonic_nanos() noexcept
{
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

void ks::platform::sleep_nanos( unsigned long long duration ) noexcept
{
	struct timespec ts;
	ts.tv_sec = (time_t)( duration / 1000000000ull );
	ts.tv_nsec = (long)( duration % 1000000000ull );
	while ( ::nanosleep( &ts, &ts ) != 0 && errno == EINTR )
	{
	}
}

bool ks::platform::path_exists( const char *path ) noexcept
{
	return path != nullptr && ::access( path, F_OK ) == 0;
}

bool ks::platform::path_is_directory( const char *path ) noexcept
{
	struct stat st;
	return path != nullptr && ::stat( path, &st ) == 0 && S_ISDIR( st.st_mode );
}

bool ks::platform::create_directory( const char *path ) noexcept
{
	return path != nullptr && ::mkdir( path, 0777 ) == 0;
}

long long ks::platform::file_modified_seconds( const char *path ) noexcept
{
	struct stat st;
	if ( path == nullptr || ::stat( path, &st ) != 0 )
		return -1;
	return (long long)st.st_mtime;
}

bool ks::platform::remove_file( const char *path, file_error *error ) noexcept
{
	file_error local = file_error::none;
	bool ok = path != nullptr && ::unlink( path ) == 0;
	if ( !ok && path != nullptr )
		local = map_errno( errno );
	if ( error != nullptr )
		*error = local;
	return ok;
}

ks::platform::mapping *ks::platform::map_file( const char *path, file_error *error ) noexcept
{
	file_error local = file_error::none;
	mapping *result = nullptr;
	int fd = ( path != nullptr ) ? ::open( path, O_RDONLY | O_CLOEXEC ) : -1;
	if ( fd < 0 )
	{
		local = map_errno( errno );
	}
	else
	{
		struct stat st;
		if ( ::fstat( fd, &st ) != 0 )
		{
			local = map_errno( errno );
		}
		else if ( !S_ISREG( st.st_mode ) || st.st_size == 0 )
		{
			local = file_error::io;
		}
		else
		{
			unsigned long long size = (unsigned long long)st.st_size;
			void *base = ::mmap( nullptr, (size_t)size, PROT_READ, MAP_PRIVATE, fd, 0 );
			if ( base == MAP_FAILED )
				local = file_error::io;
			else
				result = new mapping{ base, size };
		}
		::close( fd );
	}
	if ( error != nullptr )
		*error = local;
	return result;
}

const void *ks::platform::mapping_data( const mapping *m ) noexcept
{
	return m != nullptr ? m->base : nullptr;
}

unsigned long long ks::platform::mapping_size( const mapping *m ) noexcept
{
	return m != nullptr ? m->size : 0;
}

bool ks::platform::mapping_prefetch( const mapping *m ) noexcept
{
	if ( m == nullptr )
		return false;
	return ::madvise( m->base, (size_t)m->size, MADV_WILLNEED ) == 0;
}

void ks::platform::unmap( mapping *m ) noexcept
{
	if ( m != nullptr )
	{
		::munmap( m->base, (size_t)m->size );
		delete m;
	}
}

ks::platform::library *ks::platform::load_library( const char *path, file_error *error ) noexcept
{
	file_error local = file_error::none;
	void *handle = ::dlopen( path, RTLD_NOW | RTLD_LOCAL );
	if ( handle == nullptr )
		local = file_error::not_found;
	if ( error != nullptr )
		*error = local;
	return ( handle != nullptr ) ? new library{ handle } : nullptr;
}

void *ks::platform::library_symbol( library *lib, const char *name ) noexcept
{
	if ( lib == nullptr || name == nullptr )
		return nullptr;
	return ::dlsym( lib->handle, name );
}

void ks::platform::close_library( library *lib ) noexcept
{
	if ( lib != nullptr )
	{
		::dlclose( lib->handle );
		delete lib;
	}
}

const char *ks::platform::library_error() noexcept
{
	return ::dlerror();
}

bool ks::platform::current_directory( char *buffer, unsigned long capacity ) noexcept
{
	return buffer != nullptr && capacity != 0 && ::getcwd( buffer, (size_t)capacity ) != nullptr;
}

namespace
{

int map_signal( ks::platform::signal_kind kind ) noexcept
{
	switch ( kind )
	{
	case ks::platform::signal_kind::interrupt: return SIGINT;
	case ks::platform::signal_kind::terminate: return SIGTERM;
	case ks::platform::signal_kind::hangup: return SIGHUP;
	case ks::platform::signal_kind::user1: return SIGUSR1;
	case ks::platform::signal_kind::user2: return SIGUSR2;
	case ks::platform::signal_kind::pipe: return SIGPIPE;
	case ks::platform::signal_kind::child: return SIGCHLD;
	}
	return SIGINT;
}

} // namespace

bool ks::platform::host_name( char *buffer, unsigned long capacity ) noexcept
{
	return buffer != nullptr && capacity != 0 && ::gethostname( buffer, (size_t)capacity ) == 0;
}

bool ks::platform::set_signal_handler( signal_kind kind, signal_handler handler ) noexcept
{
	return ::signal( map_signal( kind ), handler ) != SIG_ERR;
}

ks::platform::virtual_memory *ks::platform::reserve_virtual( unsigned long long bytes ) noexcept
{
	if ( bytes == 0 )
		return nullptr;
	void *base = ::mmap( nullptr, (size_t)bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	if ( base == MAP_FAILED )
		return nullptr;
	return new virtual_memory{ base, bytes };
}

void *ks::platform::virtual_base( virtual_memory *m ) noexcept
{
	return m != nullptr ? m->base : nullptr;
}

bool ks::platform::commit_virtual( virtual_memory *m, unsigned long long offset, unsigned long long bytes ) noexcept
{
	if ( m == nullptr || offset + bytes > m->size )
		return false;
	return ::mprotect( static_cast<char *>( m->base ) + offset, (size_t)bytes, PROT_READ | PROT_WRITE ) == 0;
}

void ks::platform::release_virtual( virtual_memory *m ) noexcept
{
	if ( m != nullptr )
	{
		::munmap( m->base, (size_t)m->size );
		delete m;
	}
}

struct ks::platform::mutex
{
	pthread_mutex_t handle;
};

struct ks::platform::condition
{
	pthread_cond_t handle;
};

struct ks::platform::thread
{
	pthread_t handle;
};

struct ks::platform::tls
{
	pthread_key_t key;
};

ks::platform::mutex *ks::platform::mutex_create() noexcept
{
	mutex *m = new mutex;
	pthread_mutex_init( &m->handle, nullptr );
	return m;
}

void ks::platform::mutex_destroy( mutex *m ) noexcept
{
	if ( m != nullptr )
	{
		pthread_mutex_destroy( &m->handle );
		delete m;
	}
}

void ks::platform::mutex_lock( mutex *m ) noexcept
{
	if ( m != nullptr )
		pthread_mutex_lock( &m->handle );
}

void ks::platform::mutex_unlock( mutex *m ) noexcept
{
	if ( m != nullptr )
		pthread_mutex_unlock( &m->handle );
}

ks::platform::condition *ks::platform::condition_create() noexcept
{
	condition *c = new condition;
	pthread_cond_init( &c->handle, nullptr );
	return c;
}

void ks::platform::condition_destroy( condition *c ) noexcept
{
	if ( c != nullptr )
	{
		pthread_cond_destroy( &c->handle );
		delete c;
	}
}

void ks::platform::condition_signal( condition *c ) noexcept
{
	if ( c != nullptr )
		pthread_cond_signal( &c->handle );
}

bool ks::platform::condition_wait( condition *c, mutex *m, unsigned long long timeout_nanos, bool *timed_out ) noexcept
{
	if ( timed_out != nullptr )
		*timed_out = false;
	if ( c == nullptr || m == nullptr )
		return false;

	if ( timeout_nanos == ~0ull )
		return pthread_cond_wait( &c->handle, &m->handle ) == 0;

	struct timespec deadline;
	clock_gettime( CLOCK_REALTIME, &deadline );
	deadline.tv_sec += (time_t)( timeout_nanos / 1000000000ull );
	deadline.tv_nsec += (long)( timeout_nanos % 1000000000ull );
	if ( deadline.tv_nsec >= 1000000000L )
	{
		deadline.tv_sec += 1;
		deadline.tv_nsec -= 1000000000L;
	}

	int result = pthread_cond_timedwait( &c->handle, &m->handle, &deadline );
	if ( result == ETIMEDOUT )
	{
		if ( timed_out != nullptr )
			*timed_out = true;
		return false;
	}
	return result == 0;
}

ks::platform::thread *ks::platform::thread_create( void *( *fn )( void * ), void *arg, unsigned long long stack_bytes ) noexcept
{
	if ( fn == nullptr )
		return nullptr;
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	if ( stack_bytes != 0 )
		pthread_attr_setstacksize( &attr, (size_t)stack_bytes );
	thread *t = new thread;
	int result = pthread_create( &t->handle, &attr, fn, arg );
	pthread_attr_destroy( &attr );
	if ( result != 0 )
	{
		delete t;
		return nullptr;
	}
	return t;
}

ks::platform::thread *ks::platform::thread_current() noexcept
{
	thread_local thread self{ pthread_self() };
	return &self;
}

unsigned long long ks::platform::thread_current_id() noexcept
{
	return (unsigned long long)pthread_self();
}

bool ks::platform::thread_join( thread *t ) noexcept
{
	if ( t == nullptr )
		return false;
	bool ok = pthread_join( t->handle, nullptr ) == 0;
	delete t;
	return ok;
}

bool ks::platform::thread_id_running( unsigned long long id ) noexcept
{
	return pthread_kill( (pthread_t)id, 0 ) == 0;
}

bool ks::platform::thread_set_affinity( thread *t, unsigned long long mask ) noexcept
{
	pthread_t handle = ( t != nullptr ) ? t->handle : pthread_self();
	cpu_set_t set;
	CPU_ZERO( &set );
	for ( int i = 0; i < 64; i++ )
	{
		if ( mask & ( 1ull << i ) )
			CPU_SET( i, &set );
	}
	return pthread_setaffinity_np( handle, sizeof( set ), &set ) == 0;
}

void ks::platform::thread_exit( unsigned int code ) noexcept
{
	pthread_exit( (void *)(uintptr_t)code );
}

unsigned long long ks::platform::thread_id( thread *t ) noexcept
{
	return ( t != nullptr ) ? (unsigned long long)t->handle : 0;
}

bool ks::platform::thread_terminate( thread *t ) noexcept
{
	return t != nullptr && pthread_kill( t->handle, SIGKILL ) == 0;
}

void ks::platform::thread_yield() noexcept
{
	sched_yield();
}

int ks::platform::thread_priority( thread *t ) noexcept
{
	if ( t == nullptr )
		return 0;
	struct sched_param param;
	int policy = 0;
	if ( pthread_getschedparam( t->handle, &policy, &param ) != 0 )
		return 0;
	return param.sched_priority;
}

bool ks::platform::set_process_affinity( const bool *cpus, int count ) noexcept
{
	if ( cpus == nullptr || count <= 0 || count > CPU_SETSIZE )
		return false;
	cpu_set_t set;
	CPU_ZERO( &set );
	for ( int i = 0; i < count; i++ )
	{
		if ( cpus[i] )
			CPU_SET( i, &set );
	}
	return ::sched_setaffinity( 0, sizeof( set ), &set ) == 0;
}

bool ks::platform::set_environment( const char *name, const char *value, bool overwrite ) noexcept
{
	return name != nullptr && value != nullptr && ::setenv( name, value, overwrite ? 1 : 0 ) == 0;
}

ks::platform::heap_info ks::platform::heap_usage() noexcept
{
	const struct mallinfo2 info = ::mallinfo2();
	return heap_info{ info.arena, info.uordblks, info.hblks };
}

unsigned long ks::platform::allocation_size( void *block ) noexcept
{
	return ::malloc_usable_size( block );
}

int ks::platform::processor_count() noexcept
{
	long count = sysconf( _SC_NPROCESSORS_CONF );
	return ( count > 0 && count < 4096 ) ? (int)count : 0;
}

ks::platform::tls *ks::platform::tls_create() noexcept
{
	tls *t = new tls;
	if ( pthread_key_create( &t->key, nullptr ) != 0 )
	{
		delete t;
		return nullptr;
	}
	return t;
}

void ks::platform::tls_destroy( tls *t ) noexcept
{
	if ( t != nullptr )
	{
		pthread_key_delete( t->key );
		delete t;
	}
}

void *ks::platform::tls_get( tls *t ) noexcept
{
	return ( t != nullptr ) ? pthread_getspecific( t->key ) : nullptr;
}

bool ks::platform::tls_set( tls *t, void *value ) noexcept
{
	return t != nullptr && pthread_setspecific( t->key, value ) == 0;
}

struct ks::platform::socket
{
	int fd;
};

namespace
{

sockaddr_in to_sockaddr( const ks::platform::address &a ) noexcept
{
	sockaddr_in sa{};
	sa.sin_family = AF_INET;
	sa.sin_port = htons( a.port );
	sa.sin_addr.s_addr = htonl( a.ip );
	return sa;
}

ks::platform::socket_error map_socket_errno( int value ) noexcept
{
	switch ( value )
	{
	case EAGAIN:
	case EINPROGRESS:
		return ks::platform::socket_error::again;
	case EINTR:
		return ks::platform::socket_error::interrupted;
	case ECONNREFUSED:
		return ks::platform::socket_error::refused;
	case ECONNRESET:
	case EPIPE:
		return ks::platform::socket_error::reset;
	case EADDRINUSE:
		return ks::platform::socket_error::address_in_use;
	case EADDRNOTAVAIL:
		return ks::platform::socket_error::address_unavailable;
	case ENOTCONN:
		return ks::platform::socket_error::not_connected;
	case EMSGSIZE:
		return ks::platform::socket_error::message_too_large;
	case EAFNOSUPPORT:
	case EPROTONOSUPPORT:
		return ks::platform::socket_error::unsupported;
	case ENOENT:
		return ks::platform::socket_error::not_found;
	default:
		return ks::platform::socket_error::io;
	}
}

thread_local ks::platform::socket_error g_last_socket_error = ks::platform::socket_error::none;

void set_socket_error( ks::platform::socket_error *out, ks::platform::socket_error value ) noexcept
{
	g_last_socket_error = value;
	if ( out != nullptr )
		*out = value;
}

} // namespace

ks::platform::socket *ks::platform::open_udp( socket_error *error ) noexcept
{
	int fd = ::socket( AF_INET, SOCK_DGRAM, 0 );
	if ( fd < 0 )
	{
		set_socket_error( error, map_socket_errno( errno ) );
		return nullptr;
	}
	set_socket_error( error, socket_error::none );
	return new socket{ fd };
}

bool ks::platform::bind( socket *s, const address &local, socket_error *error ) noexcept
{
	sockaddr_in sa = to_sockaddr( local );
	if ( s == nullptr || ::bind( s->fd, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) != 0 )
	{
		set_socket_error( error, ( s == nullptr ) ? socket_error::io : map_socket_errno( errno ) );
		return false;
	}
	set_socket_error( error, socket_error::none );
	return true;
}

long ks::platform::send_to( socket *s, const address &to, const void *data, unsigned long size, socket_error *error ) noexcept
{
	sockaddr_in sa = to_sockaddr( to );
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return -1;
	}
	ssize_t sent = ::sendto( s->fd, data, size, 0, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) );
	if ( sent < 0 )
	{
		set_socket_error( error, map_socket_errno( errno ) );
		return -1;
	}
	set_socket_error( error, socket_error::none );
	return (long)sent;
}

long ks::platform::receive_from( socket *s, address *from, void *data, unsigned long size, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return -1;
	}
	sockaddr_in sa{};
	socklen_t length = sizeof( sa );
	ssize_t received = ::recvfrom( s->fd, data, size, 0, reinterpret_cast<sockaddr *>( &sa ), &length );
	if ( received < 0 )
	{
		set_socket_error( error, map_socket_errno( errno ) );
		return -1;
	}
	if ( from != nullptr )
	{
		from->ip = ntohl( sa.sin_addr.s_addr );
		from->port = ntohs( sa.sin_port );
	}
	set_socket_error( error, socket_error::none );
	return (long)received;
}

bool ks::platform::wait_readable( socket *s, unsigned long long timeout_nanos, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return false;
	}
	fd_set set;
	FD_ZERO( &set );
	FD_SET( s->fd, &set );
	struct timeval tv;
	tv.tv_sec = (time_t)( timeout_nanos / 1000000000ull );
	tv.tv_usec = (suseconds_t)( ( timeout_nanos % 1000000000ull ) / 1000ull );
	int result = ::select( s->fd + 1, &set, nullptr, nullptr, &tv );
	set_socket_error( error, ( result < 0 ) ? map_socket_errno( errno ) : socket_error::none );
	return result > 0;
}

void ks::platform::close_socket( socket *s ) noexcept
{
	if ( s != nullptr )
	{
		::close( s->fd );
		delete s;
	}
}

ks::platform::socket *ks::platform::open_tcp( socket_error *error ) noexcept
{
	int fd = ::socket( AF_INET, SOCK_STREAM, 0 );
	if ( fd < 0 )
	{
		set_socket_error( error, map_socket_errno( errno ) );
		return nullptr;
	}
	set_socket_error( error, socket_error::none );
	return new socket{ fd };
}

bool ks::platform::connect( socket *s, const address &remote, socket_error *error ) noexcept
{
	sockaddr_in sa = to_sockaddr( remote );
	if ( s == nullptr || ::connect( s->fd, reinterpret_cast<sockaddr *>( &sa ), sizeof( sa ) ) != 0 )
	{
		set_socket_error( error, ( s == nullptr ) ? socket_error::io : map_socket_errno( errno ) );
		return false;
	}
	set_socket_error( error, socket_error::none );
	return true;
}

long ks::platform::send_bytes( socket *s, const void *data, unsigned long size, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return -1;
	}
	ssize_t sent = ::send( s->fd, data, size, MSG_NOSIGNAL );
	if ( sent < 0 )
	{
		set_socket_error( error, map_socket_errno( errno ) );
		return -1;
	}
	set_socket_error( error, socket_error::none );
	return (long)sent;
}

long ks::platform::receive_bytes( socket *s, void *data, unsigned long size, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return -1;
	}
	ssize_t received = ::recv( s->fd, data, size, 0 );
	if ( received < 0 )
	{
		set_socket_error( error, map_socket_errno( errno ) );
		return -1;
	}
	set_socket_error( error, socket_error::none );
	return (long)received;
}

bool ks::platform::user_name( char *buffer, unsigned long capacity ) noexcept
{
	if ( buffer == nullptr || capacity == 0 )
		return false;
	struct passwd entry;
	struct passwd *result = nullptr;
	char scratch[4096];
	if ( ::getpwuid_r( ::getuid(), &entry, scratch, sizeof( scratch ), &result ) != 0 || result == nullptr )
		return false;
	size_t length = strlen( result->pw_name );
	if ( length >= capacity )
		length = capacity - 1;
	memcpy( buffer, result->pw_name, length );
	buffer[length] = 0;
	return true;
}

unsigned long long ks::platform::user_id() noexcept
{
	return (unsigned long long)::getuid();
}

struct ks::platform::shared_memory
{
	void *base;
	unsigned long long size;
};

ks::platform::shared_memory *ks::platform::open_shared_memory( const char *name, unsigned long long size, file_error *error ) noexcept
{
	if ( name == nullptr || size == 0 )
	{
		if ( error != nullptr )
			*error = file_error::io;
		return nullptr;
	}
	int fd = ::shm_open( name, O_RDWR, 0 );
	if ( fd < 0 )
	{
		if ( error != nullptr )
			*error = map_errno( errno );
		return nullptr;
	}
	void *base = ::mmap( nullptr, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	::close( fd );
	if ( base == MAP_FAILED )
	{
		if ( error != nullptr )
			*error = file_error::io;
		return nullptr;
	}
	if ( error != nullptr )
		*error = file_error::none;
	return new shared_memory{ base, size };
}

void *ks::platform::shared_memory_data( shared_memory *m ) noexcept
{
	return ( m != nullptr ) ? m->base : nullptr;
}

void ks::platform::close_shared_memory( shared_memory *m ) noexcept
{
	if ( m != nullptr )
	{
		::munmap( m->base, (size_t)m->size );
		delete m;
	}
}

const char *ks::platform::socket_error_text( socket_error error ) noexcept
{
	switch ( error )
	{
	case socket_error::none: return "no error";
	case socket_error::again: return "operation would block";
	case socket_error::interrupted: return "interrupted";
	case socket_error::refused: return "connection refused";
	case socket_error::reset: return "connection reset";
	case socket_error::address_in_use: return "address already in use";
	case socket_error::address_unavailable: return "address not available";
	case socket_error::not_connected: return "socket not connected";
	case socket_error::message_too_large: return "message too large";
	case socket_error::unsupported: return "not supported";
	case socket_error::not_found: return "not found";
	case socket_error::io: return "i/o error";
	}
	return "unknown error";
}

namespace
{

void report( ks::platform::socket_error *error, bool ok ) noexcept
{
	set_socket_error( error, ok ? ks::platform::socket_error::none : map_socket_errno( errno ) );
}

} // namespace

long ks::platform::peek_bytes( socket *s, void *data, unsigned long size, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return -1;
	}
	ssize_t got = ::recv( s->fd, data, size, MSG_PEEK );
	report( error, got >= 0 );
	return (long)got;
}

unsigned long ks::platform::bytes_available( socket *s ) noexcept
{
	int count = 0;
	if ( s == nullptr || ::ioctl( s->fd, FIONREAD, &count ) != 0 || count < 0 )
		return 0;
	return (unsigned long)count;
}

bool ks::platform::set_nonblocking( socket *s, socket_error *error ) noexcept
{
	int flags = ( s != nullptr ) ? ::fcntl( s->fd, F_GETFL, 0 ) : -1;
	bool ok = flags >= 0 && ::fcntl( s->fd, F_SETFL, flags | O_NONBLOCK ) == 0;
	report( error, ok );
	return ok;
}

bool ks::platform::set_socket_option( socket *s, socket_option option, int value, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return false;
	}
	int level = SOL_SOCKET;
	int name = 0;
	switch ( option )
	{
	case socket_option::keep_alive: name = SO_KEEPALIVE; break;
	case socket_option::no_delay: level = IPPROTO_TCP; name = TCP_NODELAY; break;
	case socket_option::reuse_address: name = SO_REUSEADDR; break;
	case socket_option::broadcast: name = SO_BROADCAST; break;
	case socket_option::send_buffer: name = SO_SNDBUF; break;
	case socket_option::receive_buffer: name = SO_RCVBUF; break;
	case socket_option::linger_off:
	{
		struct linger linger_value;
		linger_value.l_onoff = 0;
		linger_value.l_linger = 0;
		bool ok = ::setsockopt( s->fd, SOL_SOCKET, SO_LINGER, &linger_value, sizeof( linger_value ) ) == 0;
		report( error, ok );
		return ok;
	}
	}
	bool ok = ::setsockopt( s->fd, level, name, &value, sizeof( value ) ) == 0;
	report( error, ok );
	return ok;
}

bool ks::platform::listen_on( socket *s, int backlog, socket_error *error ) noexcept
{
	bool ok = s != nullptr && ::listen( s->fd, backlog ) == 0;
	report( error, ok );
	return ok;
}

ks::platform::socket *ks::platform::accept_connection( socket *s, address *peer, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return nullptr;
	}
	sockaddr_in sa{};
	socklen_t length = sizeof( sa );
	int fd = ::accept( s->fd, reinterpret_cast<sockaddr *>( &sa ), &length );
	report( error, fd >= 0 );
	if ( fd < 0 )
		return nullptr;
	if ( peer != nullptr )
	{
		peer->ip = ntohl( sa.sin_addr.s_addr );
		peer->port = ntohs( sa.sin_port );
	}
	return new socket{ fd };
}

bool ks::platform::wait_writable( socket *s, unsigned long long timeout_nanos, socket_error *error ) noexcept
{
	if ( s == nullptr )
	{
		set_socket_error( error, socket_error::io );
		return false;
	}
	fd_set set;
	FD_ZERO( &set );
	FD_SET( s->fd, &set );
	struct timeval tv;
	tv.tv_sec = (time_t)( timeout_nanos / 1000000000ull );
	tv.tv_usec = (suseconds_t)( ( timeout_nanos % 1000000000ull ) / 1000ull );
	int result = ::select( s->fd + 1, nullptr, &set, nullptr, &tv );
	report( error, result >= 0 );
	return result > 0;
}

bool ks::platform::open_socket_pair( socket **first, socket **second, socket_error *error ) noexcept
{
	int fds[2];
	if ( first == nullptr || second == nullptr || ::socketpair( AF_UNIX, SOCK_STREAM, 0, fds ) != 0 )
	{
		report( error, false );
		return false;
	}
	*first = new socket{ fds[0] };
	*second = new socket{ fds[1] };
	report( error, true );
	return true;
}

bool ks::platform::local_address( socket *s, address *out ) noexcept
{
	if ( s == nullptr || out == nullptr )
		return false;
	sockaddr_in sa{};
	socklen_t length = sizeof( sa );
	if ( ::getsockname( s->fd, reinterpret_cast<sockaddr *>( &sa ), &length ) != 0 )
		return false;
	out->ip = ntohl( sa.sin_addr.s_addr );
	out->port = ntohs( sa.sin_port );
	return true;
}

bool ks::platform::resolve_host( const char *name, unsigned int *ip ) noexcept
{
	if ( name == nullptr || ip == nullptr )
		return false;
	in_addr direct;
	if ( ::inet_pton( AF_INET, name, &direct ) == 1 )
	{
		*ip = ntohl( direct.s_addr );
		return true;
	}
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo *result = nullptr;
	if ( ::getaddrinfo( name, nullptr, &hints, &result ) != 0 || result == nullptr )
		return false;
	*ip = ntohl( reinterpret_cast<sockaddr_in *>( result->ai_addr )->sin_addr.s_addr );
	::freeaddrinfo( result );
	return true;
}

int ks::platform::poll_sockets( poll_entry *entries, unsigned long count, long long timeout_millis ) noexcept
{
	if ( entries == nullptr || count == 0 || count > 256 )
		return -1;
	pollfd fds[256];
	for ( unsigned long i = 0; i < count; i++ )
	{
		fds[i].fd = ( entries[i].s != nullptr ) ? entries[i].s->fd : -1;
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}
	int result = ::poll( fds, (nfds_t)count, (int)timeout_millis );
	for ( unsigned long i = 0; i < count; i++ )
		entries[i].readable = result > 0 && ( fds[i].revents & ( POLLIN | POLLHUP | POLLERR ) ) != 0;
	return result;
}

long long ks::platform::fork_process() noexcept
{
	return (long long)::fork();
}

bool ks::platform::kill_process( long long pid ) noexcept
{
	return ::kill( (pid_t)pid, SIGKILL ) == 0;
}

void ks::platform::exit_process_now( int code ) noexcept
{
	::_exit( code );
}

void ks::platform::detach_inherited_descriptors( bool reopen_standard_streams ) noexcept
{
	long limit = ::sysconf( _SC_OPEN_MAX );
	if ( limit < 0 || limit > 65536 )
		limit = 65536;
	for ( int fd = 3; fd < (int)limit; fd++ )
		::close( fd );
	if ( reopen_standard_streams )
	{
		// Explicitly close #0 (stdin), #1 (stdout), #2 (stderr) and reopen them to /dev/null to consume 0-1-2 FDs (Posix spec requires to return lowest FDs first)
		::close( 0 );
		::close( 1 );
		::close( 2 );
		::open( "/dev/null", O_RDONLY );
		::open( "/dev/null", O_RDWR );
		::open( "/dev/null", O_RDWR );
	}
}

bool ks::platform::reap_child( long long pid, bool block, child_status *status ) noexcept
{
	int raw = 0;
	pid_t result = ::waitpid( (pid_t)pid, &raw, block ? 0 : WNOHANG );
	if ( result <= 0 )
		return false;
	if ( status != nullptr )
	{
		status->pid = (long long)result;
		status->exited = WIFEXITED( raw );
		status->signaled = WIFSIGNALED( raw );
		status->code = status->exited ? WEXITSTATUS( raw ) : ( status->signaled ? WTERMSIG( raw ) : 0 );
	}
	return true;
}

ks::platform::socket_error ks::platform::last_socket_error() noexcept
{
	return g_last_socket_error;
}

namespace
{

bool fill_info( const char *path, ks::platform::file_info *info ) noexcept
{
	struct stat st;
	if ( ::stat( path, &st ) != 0 )
		return false;
	if ( info != nullptr )
	{
		info->size = (long long)st.st_size;
		info->modified_seconds = (long long)st.st_mtime;
		info->created_seconds = (long long)st.st_ctime;
		info->is_directory = S_ISDIR( st.st_mode );
		info->is_regular = S_ISREG( st.st_mode );
		info->is_writable = ( st.st_mode & S_IWUSR ) != 0;
	}
	return true;
}

bool mask_matches( const char *mask, const char *name ) noexcept
{
	if ( !strcmp( name, "." ) || !strcmp( name, ".." ) )
		return false;
	if ( !strcmp( mask, "*.*" ) )
		return true;

	while ( *mask && *name )
	{
		if ( *mask == '*' )
		{
			mask++;
			if ( !*mask )
				return true;
			while ( *name && toupper( *name ) != toupper( *mask ) )
				name++;
			if ( !*name )
				break;
		}
		else if ( *mask != '?' )
		{
			if ( toupper( *mask ) != toupper( *name ) )
				return false;
			mask++;
			name++;
			if ( !*mask && !*name )
				return true;
		}
		else
		{
			mask++;
			name++;
		}
	}
	return !*mask && !*name;
}

} // namespace

bool ks::platform::stat_path( const char *path, file_info *info ) noexcept
{
	return path != nullptr && fill_info( path, info );
}

bool ks::platform::set_path_writable( const char *path, bool writable ) noexcept
{
	struct stat st;
	if ( path == nullptr || ::stat( path, &st ) != 0 )
		return false;
	mode_t mode = writable ? ( st.st_mode | S_IWUSR ) : ( st.st_mode & ~( S_IWUSR | S_IWGRP | S_IWOTH ) );
	if ( mode == st.st_mode )
		return true;
	return ::chmod( path, mode ) == 0;
}

bool ks::platform::rename_path( const char *from, const char *to ) noexcept
{
	return from != nullptr && to != nullptr && ::rename( from, to ) == 0;
}

bool ks::platform::remove_directory( const char *path ) noexcept
{
	return path != nullptr && ::rmdir( path ) == 0;
}

struct ks::platform::directory_listing
{
	directory_entry *entries;
	unsigned long count;
};

namespace
{

int compare_entries( const void *a, const void *b ) noexcept
{
	return strcoll( static_cast<const ks::platform::directory_entry *>( a )->name, static_cast<const ks::platform::directory_entry *>( b )->name );
}

} // namespace

ks::platform::directory_listing *ks::platform::list_directory( const char *path, const char *mask ) noexcept
{
	if ( path == nullptr || mask == nullptr )
		return nullptr;
	DIR *dir = ::opendir( path );
	if ( dir == nullptr )
		return nullptr;

	unsigned long capacity = 64;
	unsigned long count = 0;
	directory_entry *entries = static_cast<directory_entry *>( ::malloc( capacity * sizeof( directory_entry ) ) );
	while ( entries != nullptr )
	{
		dirent *item = ::readdir( dir );
		if ( item == nullptr )
			break;
		if ( !mask_matches( mask, item->d_name ) )
			continue;
		if ( count == capacity )
		{
			capacity *= 2;
			directory_entry *grown = static_cast<directory_entry *>( ::realloc( entries, capacity * sizeof( directory_entry ) ) );
			if ( grown == nullptr )
			{
				::free( entries );
				entries = nullptr;
				break;
			}
			entries = grown;
		}
		directory_entry &entry = entries[count++];
		strncpy( entry.name, item->d_name, sizeof( entry.name ) - 1 );
		entry.name[sizeof( entry.name ) - 1] = 0;
		char full[4096];
		snprintf( full, sizeof( full ), "%s/%s", path, entry.name );
		if ( !fill_info( full, &entry.info ) )
			entry.info = file_info{};
	}
	::closedir( dir );
	if ( entries == nullptr )
		return nullptr;
	::qsort( entries, count, sizeof( directory_entry ), compare_entries );
	return new directory_listing{ entries, count };
}

unsigned long ks::platform::listing_count( const directory_listing *listing ) noexcept
{
	return listing != nullptr ? listing->count : 0;
}

bool ks::platform::listing_entry( const directory_listing *listing, unsigned long index, directory_entry *entry ) noexcept
{
	if ( listing == nullptr || entry == nullptr || index >= listing->count )
		return false;
	*entry = listing->entries[index];
	return true;
}

void ks::platform::free_listing( directory_listing *listing ) noexcept
{
	if ( listing != nullptr )
	{
		::free( listing->entries );
		delete listing;
	}
}

bool ks::platform::find_name_ignoring_case( const char *directory, const char *name, char *out, unsigned long capacity ) noexcept
{
	if ( directory == nullptr || name == nullptr || out == nullptr || capacity == 0 )
		return false;
	DIR *dir = ::opendir( directory );
	if ( dir == nullptr )
		return false;
	bool found = false;
	while ( dirent *item = ::readdir( dir ) )
	{
		if ( !::strcasecmp( item->d_name, name ) )
		{
			snprintf( out, capacity, "%s/%s", directory, item->d_name );
			found = true;
			break;
		}
	}
	::closedir( dir );
	return found;
}

ks::platform::lock_result ks::platform::lock_stream( void *stdio_stream, bool wait ) noexcept
{
	if ( stdio_stream == nullptr )
		return lock_result::failed;
	int fd = ::fileno_unlocked( static_cast<FILE *>( stdio_stream ) );
	if ( ::flock( fd, wait ? LOCK_EX : ( LOCK_EX | LOCK_NB ) ) == 0 )
		return lock_result::acquired;
	return errno == EWOULDBLOCK ? lock_result::would_block : lock_result::failed;
}

void ks::platform::unlock_stream( void *stdio_stream ) noexcept
{
	if ( stdio_stream != nullptr )
		::flock( ::fileno_unlocked( static_cast<FILE *>( stdio_stream ) ), LOCK_UN );
}

long ks::platform::stream_identity( void *stdio_stream ) noexcept
{
	return stdio_stream != nullptr ? (long)::fileno_unlocked( static_cast<FILE *>( stdio_stream ) ) : -1;
}

struct ks::platform::terminal_state
{
	termios saved;
};

namespace
{

void block_signals( bool block, bool include_input ) noexcept
{
	sigset_t set;
	sigemptyset( &set );
	sigaddset( &set, SIGTTOU );
	if ( include_input )
		sigaddset( &set, SIGTTIN );
	::sigprocmask( block ? SIG_BLOCK : SIG_UNBLOCK, &set, nullptr );
}

} // namespace

ks::platform::terminal_state *ks::platform::terminal_enter_raw_mode() noexcept
{
	block_signals( true, false );
	terminal_state *state = new terminal_state;
	::tcgetattr( STDIN_FILENO, &state->saved );
	termios raw = state->saved;
	raw.c_lflag &= ~( ICANON | ECHO );
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	::tcsetattr( STDIN_FILENO, TCSANOW, &raw );
	block_signals( false, false );
	return state;
}

void ks::platform::terminal_restore( terminal_state *state ) noexcept
{
	if ( state == nullptr )
		return;
	block_signals( true, false );
	::tcsetattr( STDIN_FILENO, TCSANOW, &state->saved );
	block_signals( false, false );
	delete state;
}

bool ks::platform::terminal_attached() noexcept
{
	return ::isatty( STDIN_FILENO ) != 0;
}

bool ks::platform::terminal_input_ready() noexcept
{
	fd_set set;
	FD_ZERO( &set );
	FD_SET( STDIN_FILENO, &set );
	struct timeval tv = { 0, 0 };
	return ::select( STDIN_FILENO + 1, &set, nullptr, nullptr, &tv ) != -1 && FD_ISSET( STDIN_FILENO, &set );
}

long ks::platform::terminal_read( char *buffer, unsigned long size ) noexcept
{
	return (long)::read( STDIN_FILENO, buffer, size );
}

void ks::platform::terminal_block_job_signals( bool block ) noexcept
{
	block_signals( block, true );
}

int ks::platform::terminal_columns() noexcept
{
	struct winsize ws;
	return ::ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) == 0 ? (int)ws.ws_col : 0;
}

namespace ks::platform
{

convert_result convert_text( const char *to, const char *from, char *&in, unsigned long &in_left, char *&out, unsigned long &out_left ) noexcept
{
	iconv_t converter = iconv_open( to, from );
	if ( converter == (iconv_t)-1 )
		return convert_result::no_converter;

	size_t in_bytes = in_left;
	size_t out_bytes = out_left;
	size_t result = iconv( converter, &in, &in_bytes, &out, &out_bytes );
	iconv_close( converter );
	in_left = in_bytes;
	out_left = out_bytes;
	return result == (size_t)-1 ? convert_result::failed : convert_result::ok;
}

}
