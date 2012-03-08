#include "bocl_mem.h"
//:
// \file
#include <vcl_iostream.h>
#include <vcl_cstring.h>
#include <vcl_cstdio.h>
#include <vcl_cstdlib.h>
#include <vcl_cassert.h>

bocl_mem::bocl_mem(const cl_context& context, void* buffer, unsigned num_bytes, vcl_string id)
: cpu_buf_(buffer),
  delete_cpu_(false),
  queue_(NULL),
  num_bytes_(num_bytes),
  context_(context),
  id_(id),
  is_gl_(false)
{}

bocl_mem::~bocl_mem()
{
  this->release_memory();
}

bool bocl_mem::create_buffer(const cl_mem_flags& flags)
{
  cl_int status = MEM_FAILURE;

  // Create and initialize memory objects
  buffer_ = clCreateBuffer(this->context_, flags, this->num_bytes_, this->cpu_buf_, &status);
  if (!check_val(status, MEM_FAILURE, "clCreateBuffer failed: " + this->id_))
    return MEM_FAILURE;
  return MEM_SUCCESS;
}

bool bocl_mem::create_buffer(const cl_mem_flags& flags, cl_command_queue& queue)
{
  cl_int status = MEM_FAILURE;

  // Create and initialize memory objects
  buffer_ = clCreateBuffer(this->context_, flags, this->num_bytes_, this->cpu_buf_, &status);
  if (!check_val(status, MEM_FAILURE, "clCreateBuffer failed: " + this->id_))
    return MEM_FAILURE;

  //if memory was allocated and a null pointer was passed in, store it
  if ( (flags & CL_MEM_ALLOC_HOST_PTR) && !cpu_buf_) {
#ifdef DEBUG
    vcl_cout<<"bocl_mem is allocating host pointer"<<vcl_endl;
#endif
    cpu_buf_ = clEnqueueMapBuffer(queue,
                                  buffer_,
                                  CL_TRUE,
                                  CL_MAP_READ & CL_MAP_WRITE,
                                  0,
                                  this->num_bytes_,
                                  0,NULL, NULL, &status);
    clFinish(queue);
    if (!check_val(status, MEM_FAILURE, "clEnqueueMapBuffer CL_MEM_HOST_PTR failed: " + this->id_))
      return MEM_FAILURE;

    //this buffer owns its CPU memory, make sure it gets deleted
    delete_cpu_ = true;
    queue_ = &queue;
  }
  return MEM_SUCCESS;
}

bool bocl_mem::create_image_buffer(const cl_mem_flags& flags, const cl_image_format* format,
                                   vcl_size_t width, vcl_size_t height)
{
  cl_int status = MEM_FAILURE;
  buffer_ = clCreateImage2D(this->context_, flags, format, width, height,
                            this->num_bytes_, this->cpu_buf_, &status);
  if (!check_val(status, MEM_FAILURE, "clCreateBuffer failed: " + this->id_))
    return MEM_FAILURE;
  return MEM_SUCCESS;
}

bool bocl_mem::release_memory()
{
  //release mapped memory
  if (delete_cpu_ && cpu_buf_ && queue_) {
    cl_int status = clEnqueueUnmapMemObject (*queue_, buffer_, cpu_buf_, 0, NULL, NULL);
    if (!check_val(status,MEM_FAILURE,"clEnqueueUnmapMemObject failed: " + this->id_))
      return MEM_FAILURE;
  }

  //release mem
  cl_int status = clReleaseMemObject(buffer_);
  if (!check_val(status,MEM_FAILURE,"clReleaseMemObject failed: " + this->id_))
    return MEM_FAILURE;

  return MEM_SUCCESS;
}

//: helper method to zero out gpu buffer
bool bocl_mem::zero_gpu_buffer(const cl_command_queue& cmd_queue)
{
  unsigned char* zeros = new unsigned char[this->num_bytes_]; // All values initialized to zero.
  vcl_memset(zeros, 0, this->num_bytes_);
  bool good = this->write_to_gpu_mem(cmd_queue, zeros, this->num_bytes_);
  delete[] zeros;
  return good;
}

//: helper method to initialize gpu buffer with a constant value
bool bocl_mem::init_gpu_buffer(void const* init_val, vcl_size_t value_size, cl_command_queue& cmd_queue)
{
  // sanity check on sizes
  if (this->num_bytes_ % value_size != 0) {
    vcl_cerr << "ERROR: bocl_mem::init_gpu_buffer(): value_size does not divide evenly into buffer size." <<vcl_endl;
    return MEM_FAILURE;
  }
  unsigned char* init_buff = new unsigned char[this->num_bytes_];
  unsigned int num_values = this->num_bytes_ / value_size;
  vcl_cout << "value_size = " << value_size << vcl_endl
           << "num_values = " << num_values << vcl_endl;

  // fill in buffer with copies of init value
  unsigned char* buff_ptr = init_buff;
  for (unsigned int i=0; i<num_values; ++i) {
    vcl_memcpy(buff_ptr, init_val, value_size);
    buff_ptr += value_size;
  }
  // copy buffer over to GPU
  ceEvent_ = 0;
  cl_int status = MEM_FAILURE;
  status = clEnqueueWriteBuffer(cmd_queue,
                                this->buffer_,
                                CL_TRUE,          //True=BLocking, False=NonBlocking
                                0,
                                this->num_bytes_,
                                init_buff,
                                0,                //cl_uint num_events_in_wait_list
                                0,
                                &ceEvent_);
  delete[] init_buff;
  if (!check_val(status,MEM_FAILURE,"clEnqueueWriteBuffer (INIT BUFFER) failed: " + this->id_ + error_to_string(status)))
    return MEM_FAILURE;
  return MEM_SUCCESS;
}

//: write to command queue
bool bocl_mem::write_to_buffer(const cl_command_queue& cmd_queue)
{
  if (!is_gl_) 
    return this->write_to_gpu_mem(cmd_queue, this->cpu_buf_, this->num_bytes_);
  return true;
}

//: read from command queue buffer...
bool bocl_mem::read_to_buffer(const cl_command_queue& cmd_queue)
{
  if (!is_gl_)
    return this->read_from_gpu_mem(cmd_queue, this->cpu_buf_, this->num_bytes_);
  return true;
}

bool bocl_mem::write_to_gpu_mem(const cl_command_queue& cmd_queue, void* buff, vcl_size_t size)
{
  assert(size <= this->num_bytes_);
  ceEvent_ = 0;
  cl_int status = MEM_FAILURE;
  status = clEnqueueWriteBuffer(cmd_queue,
                                this->buffer_,
                                CL_TRUE,          //True=BLocking, False=NonBlocking
                                0,
                                size,
                                buff,
                                0,                //cl_uint num_events_in_wait_list
                                0,
                                &ceEvent_);
  if (!check_val(status,MEM_FAILURE,"clEnqueueWriteBuffer failed: " + this->id_ + error_to_string(status)))
    return MEM_FAILURE;
  return MEM_SUCCESS;
}

bool bocl_mem::read_from_gpu_mem(const cl_command_queue& cmd_queue, void* buff, vcl_size_t size)
{
  assert(size <= this->num_bytes_);
  int status = MEM_FAILURE;
  status= clEnqueueReadBuffer(cmd_queue,
                              this->buffer_,
                              CL_TRUE,
                              0,
                              size,
                              buff,
                              0,
                              NULL,
                              &ceEvent_);
  if (!check_val(status,MEM_FAILURE,"clEnqueueReadBuffer failed: " + this->id_ + error_to_string(status)))
    return MEM_FAILURE;
  return MEM_SUCCESS;
}

//: write to command queue
bool bocl_mem::write_to_buffer_async(const cl_command_queue& cmd_queue)
{
  if (!is_gl_) {
    cl_int status = MEM_FAILURE;
    status = clEnqueueWriteBuffer(cmd_queue,
                                  this->buffer_,
                                  CL_FALSE,          //True=BLocking, False=NonBlocking
                                  0,
                                  this->num_bytes_,
                                  this->cpu_buf_,
                                  0,                //cl_uint num_events_in_wait_list
                                  0,
                                  &ceEvent_);
    if (!check_val(status,MEM_FAILURE,"clEnqueueWriteBuffer (async) failed: " + this->id_))
      return MEM_FAILURE;
    return MEM_SUCCESS;
  }
  return true;
}

//: finish write to buffer using clWaitForEvent
bool bocl_mem::finish_write_to_buffer(const cl_command_queue& cmd_queue)
{
  if (!is_gl_) {
    cl_int status = MEM_FAILURE;
    status = clWaitForEvents(1, &event_);
    if (!check_val(status,MEM_FAILURE,"clWaitForEvents failed: " + this->id_ + error_to_string(status)))
      return MEM_FAILURE;
    return MEM_SUCCESS;
  }
  return true;
}

//: THIS REQUIRES the queue to be finished
float bocl_mem::exec_time()
{
  cl_ulong tend, tstart;
  int status = clGetEventProfilingInfo(ceEvent_,CL_PROFILING_COMMAND_END,sizeof(cl_ulong),&tend,0);
  status = clGetEventProfilingInfo(ceEvent_,CL_PROFILING_COMMAND_START,sizeof(cl_ulong),&tstart,0);
  if ( !check_val(status,CL_SUCCESS,"clFinish/ProfilingInfo failed (" + id_ + ") " +error_to_string(status)) )
    return false;

  //store execution time
  return 1e-6f*float(tend - tstart);
}

//---I/O------------------------------------------------------------------------
// Binary write boxm_update_bit_scene_manager scene to stream
void vsl_b_write(vsl_b_ostream& /*os*/, bocl_mem const& /*scene*/) {}
void vsl_b_write(vsl_b_ostream& /*os*/, const bocl_mem* & /*p*/) {}
void vsl_b_write(vsl_b_ostream& /*os*/, bocl_mem_sptr& /*sptr*/) {}
void vsl_b_write(vsl_b_ostream& /*os*/, bocl_mem_sptr const& /*sptr*/) {}

// Binary load boxm_update_bit_scene_manager scene from stream.
void vsl_b_read(vsl_b_istream& /*is*/, bocl_mem & /*scene*/) {}
void vsl_b_read(vsl_b_istream& /*is*/, bocl_mem* /*p*/) {}
void vsl_b_read(vsl_b_istream& /*is*/, bocl_mem_sptr& /*sptr*/) {}
void vsl_b_read(vsl_b_istream& /*is*/, bocl_mem_sptr const& /*sptr*/) {}
