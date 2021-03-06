
#pragma once

#include "../public/locks.h"
#include <list>
#include <vector>
#include <process.h>

using namespace std;

namespace tp_ipc_peer_namespace
{
	class ctpool;
}

/// 线程池
namespace tp_ipc_peer_namespace
{
	/// 接口
	class task_object
	{
	public:
		virtual ~task_object(){}
		virtual void exec() = 0;
	};

	template< typename Functor>
	class task 
		: public task_object
	{

		/// 禁止操作
	private:
		task( const task &);
		task & operator =( const task & );

	public:
		typedef Functor functor_type;

		task( functor_type* functor)
			: functor_( functor )
		{ }

		~task()
		{
			if( functor_ !=NULL )
				delete functor_;
		}

		/// 执行
		virtual void exec()
		{
			if( !(*functor_)() )
			{
				char szError[10] = "";
				//std::cout << szError << std::endl;
			}
		}

	private:
		Functor* functor_;
		
	};

	class ctpool
	{
		typedef ctpool self_type;
		
	public:
		ctpool(void)
			:tpool_running_( true )
		{ 
			pool_Count = 1;
			task_result_.clear();
			task_container_.clear();
		}
		ctpool ( unsigned threadsize )
			:tpool_running_(true)
		{
			pool_Count = threadsize;
			task_result_.clear();
			task_container_.clear();
		}

		template< typename Function>
		void push( Function * f)
		{
			/// 枷锁
			task_container_.push_back( new tp_ipc_peer_namespace::task<Function>( f ) );
		}

		template< typename Function>
		void push_back( Function * f)
		{
			/// 枷锁
			task_result_.push_back( new tp_ipc_peer_namespace::task<Function>( f ) );
		}

		/*void */
		
		void Start( long size )
		{
			Ras_running_ = false;
			_m_start_threads( 1 );
			//_m_start_threads_s(1);
		}

		void RasStates(bool  runing=true)
		{
			Ras_running_ =  true;
		}

		~ctpool(void){}


		bool   IsExit()const		{	return pool_Count <= 0 ; }
		bool   IsRas()const			{   return Ras_running_;	 }

	private:

		/// 创建线程池
		void _m_start_threads( unsigned size )
		{
			if ( size == 0 )
				size = 4;

			for ( unsigned i = 0 ; i < size ; i++)
			{
				AfxSocketInit();
				_beginthreadex( 0 , 0 , &ctpool::_m_work_thread , this , NULL ,NULL);
			}

			pool_Count = size ;
		}
		

		void _m_start_threads_s( unsigned size )
		{
			if ( size == 0 )
				size = 4;

			for ( unsigned i = 0 ; i < size ; i++)
			{
				AfxSocketInit();
				::_beginthreadex( 0 , 0 , &ctpool::_m_work_thread_s , this , NULL ,NULL);
			}

			pool_Count += size ;
		}

		/// 唤醒
		void _m_wakeup()
		{
			HANDLE handle = 0;

			/// 对共享区 枷锁
			tlock_.enter();
			std::vector<tinfo_type>::iterator it = threads_.begin(), end = threads_.end();

			for ( ; it != end ; ++it )
			{
				if ( it->state == 0 ) /// 在等待状态
				{
					handle	  =  it->handle ;
					it->state = 1;
					break;
				}
			}
			tlock_.leave();

			while ( ::ResumeThread( handle ) != 1)
				;
		}

		/// 挂起某个线程
		void _m_suspend()
		{
			unsigned tid = ::GetCurrentThreadId();
			HANDLE   handle = 0;

			tlock_.enter();

			/// 对共享区 枷锁
			tlock_.enter();
			std::vector<tinfo_type>::iterator it = threads_.begin(), end = threads_.end();

			for ( ; it != end ; ++it )
			{
				if ( it->tid == tid ) /// 运行ID
				{
					handle	  =  it->handle ;
					it->state = 0;
					break;
				}
			}
			tlock_.leave();

			/// 挂起
			if ( handle)
			{
				::SuspendThread( handle );
			}
		}

		/// 获取task
		tp_ipc_peer_namespace::task_object * _m_read_task()
		{
			//while( tpool_running_ )
			{
				tp_ipc_peer_namespace::task_object * task = NULL;
				
				/// 对共享区 枷锁
				//task_lock_.enter();
				if ( task_container_.size() )
				{
					task = *( task_container_.begin() );
					task_container_.erase( task_container_.begin() );
				}
				//task_lock_.leave();

				if ( task )
				{
					return task;
				}

				//break;
			}
			return NULL;
		}

		tp_ipc_peer_namespace::task_object * _m_read_task_s()
		{
			//while( tpool_running_ && Ras_running_ )
			{
				tp_ipc_peer_namespace::task_object * task = NULL;

				/// 对共享区 枷锁
				//task_lock_.enter();
				if ( task_result_.size() )
				{
					task = *( task_result_.begin() );
					task_result_.erase( task_result_.begin() );
				}
				//task_lock_.leave();

				if ( task )
				{
					return task;
				}

				//break;
			}
			return NULL;
		}

	private:
		static unsigned __stdcall _m_work_thread(void * arg)
		{					 			
			self_type & self = *reinterpret_cast<self_type*>(arg);
			tp_ipc_peer_namespace::task_object * task = 0;

			while( true )
			{
				if( Ras_running_ )
					continue;
				task = self._m_read_task();
				if ( task )
				{
					task->exec();

					//Sleep( 1 );
					delete task ;
					task = 0;
				}
				else
					break;
			}
			pool_Count -- ;
			::_endthreadex( 0 );
			return 0;
		}

		static unsigned __stdcall _m_work_thread_s(void * arg)
		{

			self_type & self = *reinterpret_cast<self_type*>(arg);
			tp_ipc_peer_namespace::task_object * task = 0;

			while( true )
			{
				if( Ras_running_ )
					continue;
				task = self._m_read_task_s();
				if ( task )
				{
					task->exec();

					//Sleep( 1 );
					delete task ;
					task = 0;
				}
				else
					break;
			}
			pool_Count -- ;
			::_endthreadex( 0 );
			return 0;
		}

	private:
		/// 线程信息
		struct tinfo_type
		{
			HANDLE			handle;
			unsigned		tid;
			unsigned long	state;  // 0 = sleep;
		};

		/// user define var
	private:
		/// 线程运行状态	volatile
		bool				tpool_running_;
		/// 一个临界区类
		sync::csectionlock			tlock_;                
		/// 线程信息
		std::vector<tinfo_type>		threads_;		
		/// 
		sync::csectionlock			task_lock_;
		/// 一个回调函数
		std::list<task_object* >    task_container_;
		/// 一个回调函数
		std::list<task_object* >	task_result_;
		/// 拨号阶段
		volatile	static  bool	Ras_running_;
		
		public:
			/// 线程数量
			volatile	static  long	pool_Count;

	};
	//volatile	bool  tp_ipc_peer_namespace::ctpool::Ras_running_ = false;
}