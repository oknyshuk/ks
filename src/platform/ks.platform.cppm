// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

export module ks.platform;

export namespace ks::platform
{

unsigned long long monotonic_nanos() noexcept;
unsigned long long process_id() noexcept;
int last_errno() noexcept;
void clear_errno() noexcept;
void sleep_nanos( unsigned long long duration ) noexcept;

enum class file_error
{
	none,
	not_found,
	denied,
	io,
};

bool path_exists( const char *path ) noexcept;
bool path_is_directory( const char *path ) noexcept;
bool create_directory( const char *path ) noexcept;
long long file_modified_seconds( const char *path ) noexcept;
bool remove_file( const char *path, file_error *error ) noexcept;

enum class file_mode
{
	read,
	write,
	write_keep,
	read_write,
};

struct file_info
{
	long long size;
	long long modified_seconds;
	long long created_seconds;
	bool is_directory;
	bool is_regular;
	bool is_writable;
};

bool stat_path( const char *path, file_info *info ) noexcept;
bool set_path_writable( const char *path, bool writable ) noexcept;
bool rename_path( const char *from, const char *to ) noexcept;
bool remove_directory( const char *path ) noexcept;

struct directory_entry
{
	char name[512];
	file_info info;
};

struct directory_listing;

directory_listing *list_directory( const char *path, const char *mask ) noexcept;
unsigned long listing_count( const directory_listing *listing ) noexcept;
bool listing_entry( const directory_listing *listing, unsigned long index, directory_entry *entry ) noexcept;
void free_listing( directory_listing *listing ) noexcept;
bool find_name_ignoring_case( const char *directory, const char *name, char *out, unsigned long capacity ) noexcept;

enum class lock_result
{
	acquired,
	would_block,
	failed,
};

lock_result lock_stream( void *stdio_stream, bool wait ) noexcept;
void unlock_stream( void *stdio_stream ) noexcept;
long stream_identity( void *stdio_stream ) noexcept;

struct file;

file *open_file( const char *path, file_mode mode, file_error *error ) noexcept;
long read_file( file *f, void *buffer, unsigned long size, file_error *error ) noexcept;
long write_file( file *f, const void *data, unsigned long size, file_error *error ) noexcept;
long long seek_file( file *f, long long offset, int origin, file_error *error ) noexcept;
unsigned long long file_size( file *f ) noexcept;
bool set_file_times( file *f, long long access_seconds, long long modified_seconds ) noexcept;
bool set_file_mode( file *f, unsigned int mode ) noexcept;
bool lock_file( file *f, bool exclusive ) noexcept;
void close_file( file *f ) noexcept;
file *duplicate_file( file *f ) noexcept;

struct mapping;

mapping *map_file( const char *path, file_error *error ) noexcept;
const void *mapping_data( const mapping *m ) noexcept;
unsigned long long mapping_size( const mapping *m ) noexcept;
bool mapping_prefetch( const mapping *m ) noexcept;
void unmap( mapping *m ) noexcept;

struct virtual_memory;

virtual_memory *reserve_virtual( unsigned long long bytes ) noexcept;
void *virtual_base( virtual_memory *m ) noexcept;
bool commit_virtual( virtual_memory *m, unsigned long long offset, unsigned long long bytes ) noexcept;
void release_virtual( virtual_memory *m ) noexcept;

struct library;

library *load_library( const char *path, file_error *error ) noexcept;
void *library_symbol( library *lib, const char *name ) noexcept;
void close_library( library *lib ) noexcept;
const char *library_error() noexcept;

bool current_directory( char *buffer, unsigned long capacity ) noexcept;

struct terminal_state;

terminal_state *terminal_enter_raw_mode() noexcept;
void terminal_restore( terminal_state *state ) noexcept;
bool terminal_attached() noexcept;
bool terminal_input_ready() noexcept;
long terminal_read( char *buffer, unsigned long size ) noexcept;
void terminal_block_job_signals( bool block ) noexcept;
int terminal_columns() noexcept;

long long fork_process() noexcept;
bool kill_process( long long pid ) noexcept;
[[noreturn]] void exit_process_now( int code ) noexcept;
void detach_inherited_descriptors( bool reopen_standard_streams ) noexcept;

struct child_status
{
	long long pid;
	int code;
	bool exited;
	bool signaled;
};

bool reap_child( long long pid, bool block, child_status *status ) noexcept;
bool host_name( char *buffer, unsigned long capacity ) noexcept;

enum class signal_kind
{
	interrupt,
	terminate,
	hangup,
	user1,
	user2,
	pipe,
	child,
};

using signal_handler = void (*)( int );

bool set_signal_handler( signal_kind kind, signal_handler handler ) noexcept;

struct mutex;
struct condition;
struct thread;
struct tls;

mutex *mutex_create() noexcept;
void mutex_destroy( mutex *m ) noexcept;
void mutex_lock( mutex *m ) noexcept;
void mutex_unlock( mutex *m ) noexcept;

condition *condition_create() noexcept;
void condition_destroy( condition *c ) noexcept;
void condition_signal( condition *c ) noexcept;
bool condition_wait( condition *c, mutex *m, unsigned long long timeout_nanos, bool *timed_out ) noexcept;

thread *thread_create( void *( *fn )( void * ), void *arg, unsigned long long stack_bytes ) noexcept;
thread *thread_current() noexcept;
unsigned long long thread_current_id() noexcept;
bool thread_join( thread *t ) noexcept;
bool thread_id_running( unsigned long long id ) noexcept;
bool thread_set_affinity( thread *t, unsigned long long mask ) noexcept;
void thread_exit( unsigned int code ) noexcept;
unsigned long long thread_id( thread *t ) noexcept;
bool thread_terminate( thread *t ) noexcept;
void thread_yield() noexcept;
int processor_count() noexcept;
bool set_environment( const char *name, const char *value, bool overwrite ) noexcept;

struct heap_info
{
	unsigned long arena;
	unsigned long in_use;
	unsigned long mapped_blocks;
};

heap_info heap_usage() noexcept;
unsigned long allocation_size( void *block ) noexcept;
bool set_process_affinity( const bool *cpus, int count ) noexcept;
int thread_priority( thread *t ) noexcept;

tls *tls_create() noexcept;void tls_destroy( tls *t ) noexcept;
void *tls_get( tls *t ) noexcept;
bool tls_set( tls *t, void *value ) noexcept;

struct address
{
	unsigned int ip;    // host byte order
	unsigned short port; // host byte order
};

enum class socket_error
{
	none,
	again,
	interrupted,
	refused,
	reset,
	address_in_use,
	address_unavailable,
	not_connected,
	message_too_large,
	unsupported,
	not_found,
	io,
};

const char *socket_error_text( socket_error error ) noexcept;
socket_error last_socket_error() noexcept;

enum class socket_option
{
	keep_alive,
	no_delay,
	reuse_address,
	broadcast,
	send_buffer,
	receive_buffer,
	linger_off,
};

struct socket;

socket *open_udp( socket_error *error ) noexcept;
bool bind( socket *s, const address &local, socket_error *error ) noexcept;
long send_to( socket *s, const address &to, const void *data, unsigned long size, socket_error *error ) noexcept;
long receive_from( socket *s, address *from, void *data, unsigned long size, socket_error *error ) noexcept;
bool wait_readable( socket *s, unsigned long long timeout_nanos, socket_error *error ) noexcept;
void close_socket( socket *s ) noexcept;

socket *open_tcp( socket_error *error ) noexcept;
bool connect( socket *s, const address &remote, socket_error *error ) noexcept;
long send_bytes( socket *s, const void *data, unsigned long size, socket_error *error ) noexcept;
long receive_bytes( socket *s, void *data, unsigned long size, socket_error *error ) noexcept;
long peek_bytes( socket *s, void *data, unsigned long size, socket_error *error ) noexcept;
unsigned long bytes_available( socket *s ) noexcept;

bool set_nonblocking( socket *s, socket_error *error ) noexcept;
bool set_socket_option( socket *s, socket_option option, int value, socket_error *error ) noexcept;
bool listen_on( socket *s, int backlog, socket_error *error ) noexcept;
socket *accept_connection( socket *s, address *peer, socket_error *error ) noexcept;
bool wait_writable( socket *s, unsigned long long timeout_nanos, socket_error *error ) noexcept;
bool open_socket_pair( socket **first, socket **second, socket_error *error ) noexcept;
bool local_address( socket *s, address *out ) noexcept;
bool resolve_host( const char *name, unsigned int *ip ) noexcept;

struct poll_entry
{
	socket *s;
	bool readable;
};

int poll_sockets( poll_entry *entries, unsigned long count, long long timeout_millis ) noexcept;

bool user_name( char *buffer, unsigned long capacity ) noexcept;
unsigned long long user_id() noexcept;

struct shared_memory;

shared_memory *open_shared_memory( const char *name, unsigned long long size, file_error *error ) noexcept;
void *shared_memory_data( shared_memory *m ) noexcept;
void close_shared_memory( shared_memory *m ) noexcept;

enum class convert_result
{
	no_converter,
	failed,
	ok,
};

convert_result convert_text( const char *to, const char *from, char *&in, unsigned long &in_left, char *&out, unsigned long &out_left ) noexcept;

} // namespace ks::platform
