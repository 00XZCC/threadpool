#include"threadpool.h"
#include<functional>
#include<thread>
#include<iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60;//�߳�������ʱ�䣬��λΪ��

ThreadPool::ThreadPool()
	: initThreadSize_(0)
	, idleThreadSize_(0)
	, taskSize_(0)
	, curThreadSize_(0)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
{}


ThreadPool::~ThreadPool()
{
	//������û���ڶ��Ϸ����ڴ棬���Բ���ʵ��
	//����Ҳ����ʡ�ԣ�C++��̹淶�У�ֻҪ�й��캯������Ҫ�ж�Ӧ����������
	//��ʵ��ҲҪ��д������Ϊ���Ժ���չ���ܾ���Ҫ�õ�������


	isPoolRunning_ = false;
	//notEmpty_.notify_all();//ԭ���Ļ����������λ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();//�Ľ���ģ��������������λ�á��̳߳���ԴҪ�����ˣ�����Щû�й����������ȴ����̸߳����Ѳ�����������
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });//�߳������е��߳�Ԫ������Ϊ0���ͱ�ʾ�����߳���Դ����������
}

//�����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

//����task�������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshhold;
}

void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)//ֻ��cachedģʽ�������߳�������ֵ��fixedģʽ��û��Ҫ��
		threadSizeThreshHold_ = threshhold;
}

//���̳߳��ύ����  �û����øýӿڣ��������������������
Result ThreadPool::submitTask(std::shared_ptr<Task>sp)
{
	/*if (checkRunningState())
		return;*/

		// ��ȡ��
	std::unique_lock<std::mutex>lock(taskQueMtx_);

	// �̵߳�ͨ�� �ж���������Ƿ��п��� 
	while (taskQue_.size() == taskQueMaxThreshHold_)
	{
		//û���࣬�ͽ����������ȴ�,���������������1�룬�����ж��ύ����ʧ�ܣ�����ֱ�ӷ���
		notFull_.wait_for(lock, std::chrono::seconds(1));
		//һ����ӡ������Ϣ��־��������
		std::cout << "task queue is full, submit task fail!" << std::endl;
		return Result(sp, false);
	}

	// ����п��࣬�����������������У�������������+1
	taskQue_.emplace(sp);
	taskSize_++;

	// ��Ϊ�·�������������п϶������ˣ���notEmpty_�Ͻ���֪ͨ
	notEmpty_.notify_all();

	//cachedģʽ��Ҫͨ��cachedģʽ��������������̣߳���Ҫ���ж�����
	//ʹ�ó����ǣ���Ҫ��������ģ�С�����������Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̳߳���
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_ //�����������ڿ����߳�����
		&& curThreadSize_ < threadSizeThreshHold_//��ǰ�߳��������ܳ����߳�������ֵ
		)
	{
		//�������߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//��������߳�Ҫ�������޸��̳߳�������̵߳�����
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize_++;//�մ����������߳�Ҳ���ڿ����߳�
	}

	std::cout << "submitTask success" << endl;
	return Result(sp);//����ִ�н����Ҳ���������result����
}

//�����̳߳�
void ThreadPool::start(int initThreadSize)
{
	//�̳߳�һ�����������޸��̳߳ص�����״̬
	isPoolRunning_ = true;

	//��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	//�����̶߳���
	for (size_t i = 0; i < initThreadSize_; ++i)
	{
		// ����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳����൱�ڰ�ÿ��threadFunc�������ŵ����̶߳�����
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		//��������Ԫ���漰���������죬unique_ptrֻ֧���ƶ����죬������Ҫ�õ��ƶ�����
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	//���������߳�
	for (size_t i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start();//Thread���е�start��ͨ�����start����ȥִ��һ���̺߳��������������߳�
		idleThreadSize_++;//�̳߳ظ�������ʱ�򣬶��ǿ����̣߳�����һ���������̼߳�����+1
	}
}

//�̺߳������̳߳���ִ��������߳�
void ThreadPool::threadFunc(int threadid)
{
	//std::cout << std::this_thread::get_id() << std::endl;
	auto lastTime = std::chrono::high_resolution_clock().now();//�߳̿�ʼִ������ʱ��ʱ��

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			// �Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����..." << std::endl;

			// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳�
			// �������յ�������initThreadSize_�������߳�Ҫ���л��գ�
			// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60s

			// ÿһ���з���һ��   ��ô���֣���ʱ���أ������������ִ�з���

			//ȫ������ִ����ɣ��̳߳��е��߳���Դ���ܿ�ʼ����
			//��δ��뾭���˷����޸ģ����վ�����ѻ����߳���Դ�Ĵ���д��һ���ط�
			//ͬ���Ĵ��������ͬһ���ط����ֶ�Σ��������߼���ͳһ�ܹ�
			while (taskQue_.size() == 0)
			{
				// �̳߳�Ҫ�����������߳���Դ
				if (!isPoolRunning_)
				{
					threads_.erase(threadid); // std::this_thread::getid()
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
						<< std::endl;
					exitCond_.notify_all();
					return; // �̺߳����������߳̽���
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
					// ������������ʱ������
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							// ��ʼ���յ�ǰ�߳�
							// ��¼�߳���������ر�����ֵ�޸�
							// ���̶߳�����߳��б�������ɾ��   û�а취 threadFunc��=��thread����
							// threadid => thread���� => ɾ��
							threads_.erase(threadid); // std::this_thread::getid()
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
								<< std::endl;
							return;
						}
					}
				}
				else
				{
					// �ȴ�notEmpty����
					notEmpty_.wait(lock);
				}

				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadid); // std::this_thread::getid()
				//	std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
				//		<< std::endl;
				//	exitCond_.notify_all();
				//	return; // �����̺߳��������ǽ�����ǰ�߳���!
				//}
			}

			idleThreadSize_--;

			std::cout << "tid:" << std::this_thread::get_id()
				<< "��ȡ����ɹ�..." << std::endl;

			// �����������ȡһ���������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			// �����Ȼ��ʣ�����񣬼���֪ͨ�������߳�ִ������
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			// ȡ��һ�����񣬽���֪ͨ��֪ͨ���Լ����ύ��������
			notFull_.notify_all();
		} // �����Ӧ�ð����ͷŵ�

		// ��ǰ�̸߳���ִ���������
		if (task != nullptr)
		{
			// task->run(); // ִ�����񣻰�����ķ���ֵsetVal��������Result
			task->exec();
		}

		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
	}
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}

///////////////�̷߳���ʵ��
int Thread::generateId_ = 0;

void Thread::start()
{
	//����һ���̶߳�����ִ��һ���̺߳���
	//�ֲ��̶߳������start����������֮�󣬾ͻᱻ�����ˣ�Ϊ�˲����߳������̶߳�������������ͷţ����³�������ҵ�
	//�Ͱ��̶߳�����̺߳��������뿪��,���÷����̣߳������̺߳����Ͳ��������̶߳��������������ֹ��
	std::thread t(func_, threadId_);
	t.detach();
}

int Thread::getId()const
{
	return threadId_;
}

Thread::Thread(ThreadFunc func)
	:func_(func)
	, threadId_(generateId_++)
{}

Thread::~Thread()
{

}

///////////////Result����ʵ��
Result::Result(std::shared_ptr<Task> task, bool isvalid)
	:task_(task)
	, isValid_(isvalid)
{
	task_->setResult(this);
}

Any Result::get()//������������û����õ�
{
	if (!isValid_)
		return "";
	sem_.wait();// task�������û��ִ���꣬����������û����̣߳�������ͨ����ʵ�ֵ��ź�����wait��������
	return std::move(any_);//�������챻delete�ˣ������������ƶ�����
}

//��������Ĺ����ǣ��������ִ�н����ͨ��setVal()����Result����
//�������Ӧ��������ִ�д����ã���������˵����threadFunc()�����run()
//��run()����ֻ�ܸ���ִ�������������Ӧ����run()�󣬱����ã��������ִ�н����ͨ��setVal()����Result����
//����Ҫ��run()��һ����װ��������͸���װ��exec()����
//��Ҳ��oop��һ��˼�룬�������ĳ��������չ�Ĺ��ܣ�����ֱ��д���麯������԰�����麯����һ����װ
//�ڷ�װ��ĺ������ٵ����麯��������ʵ���¹��ܣ�ͬʱһ������ʵ�ֶ�̬��
void Result::setVal(Any any)
{
	this->any_ = std::move(any);
	sem_.post();
}

////////////  Task����ʵ��
Task::Task()
	:result_(nullptr)
{}

void Task::exec()
{
	if (result_ != nullptr)
		result_->setVal(run());//���﷢����̬���á���װ��exec��Ŀ����Ϊ�˿��԰�����ķ���ֵsetVal��������Result
}


void Task::setResult(Result* result)
{
	result_ = result;
}


//#include "threadpool.h"
//
//#include <functional>
//#include <thread>
//#include <iostream>
//
//const int TASK_MAX_THRESHHOLD = INT32_MAX;
//const int THREAD_MAX_THRESHHOLD = 1024;
//const int THREAD_MAX_IDLE_TIME = 60; // ��λ����

//// �̳߳ع���
//ThreadPool::ThreadPool()
//	: initThreadSize_(0)
//	, taskSize_(0)
//	, idleThreadSize_(0)
//	, curThreadSize_(0)
//	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
//	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
//	, poolMode_(PoolMode::MODE_FIXED)
//	, isPoolRunning_(false)
//{}
//
//// �̳߳�����
//ThreadPool::~ThreadPool()
//{
//	isPoolRunning_ = false;
//
//	// �ȴ��̳߳��������е��̷߳���  ������״̬������ & ����ִ��������
//	std::unique_lock<std::mutex> lock(taskQueMtx_);
//	notEmpty_.notify_all();
//	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
//}
//
//// �����̳߳صĹ���ģʽ
//void ThreadPool::setMode(PoolMode mode)
//{
//	if (checkRunningState())
//		return;
//	poolMode_ = mode;
//}
//
//// ����task�������������ֵ
//void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
//{
//	if (checkRunningState())
//		return;
//	taskQueMaxThreshHold_ = threshhold;
//}
//
//// �����̳߳�cachedģʽ���߳���ֵ
//void ThreadPool::setThreadSizeThreshHold(int threshhold)
//{
//	if (checkRunningState())
//		return;
//	if (poolMode_ == PoolMode::MODE_CACHED)
//	{
//		threadSizeThreshHold_ = threshhold;
//	}
//}
//
//// ���̳߳��ύ����    �û����øýӿڣ��������������������
//Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
//{
//	// ��ȡ��
//	std::unique_lock<std::mutex> lock(taskQueMtx_);
//
//	// �̵߳�ͨ��  �ȴ���������п���   wait   wait_for   wait_until
//	// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����
//	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
//		[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
//	{
//		// ��ʾnotFull_�ȴ�1s�֣�������Ȼû������
//		std::cerr << "task queue is full, submit task fail." << std::endl;
//		// return task->getResult();  // Task  Result   �߳�ִ����task��task����ͱ���������
//		return Result(sp, false);
//	}
//
//	// ����п��࣬������������������
//	taskQue_.emplace(sp);
//	taskSize_++;
//
//	// ��Ϊ�·�������������п϶������ˣ���notEmpty_�Ͻ���֪ͨ���Ͽ�����߳�ִ������
//	notEmpty_.notify_all();
//
//	// cachedģʽ ������ȽϽ��� ������С��������� ��Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̳߳���
//	if (poolMode_ == PoolMode::MODE_CACHED
//		&& taskSize_ > idleThreadSize_
//		&& curThreadSize_ < threadSizeThreshHold_)
//	{
//		std::cout << ">>> create new thread..." << std::endl;
//
//		// �����µ��̶߳���
//		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
//		int threadId = ptr->getId();
//		threads_.emplace(threadId, std::move(ptr));
//		// �����߳�
//		threads_[threadId]->start();
//		// �޸��̸߳�����صı���
//		curThreadSize_++;
//		idleThreadSize_++;
//	}
//
//	// ���������Result����
//	return Result(sp);
//	// return task->getResult();
//}
//
//// �����̳߳�
//void ThreadPool::start(int initThreadSize)
//{
//	// �����̳߳ص�����״̬
//	isPoolRunning_ = true;
//
//	// ��¼��ʼ�̸߳���
//	initThreadSize_ = initThreadSize;
//	curThreadSize_ = initThreadSize;
//
//	// �����̶߳���
//	for (size_t i = 0; i < initThreadSize_; i++)
//	{
//		// ����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
//		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
//		int threadId = ptr->getId();
//		threads_.emplace(threadId, std::move(ptr));
//		// threads_.emplace_back(std::move(ptr));
//	}
//
//	// ���������߳�  std::vector<Thread*> threads_;
//	for (size_t i = 0; i < initThreadSize_; i++)
//	{
//		threads_[i]->start(); // ��Ҫȥִ��һ���̺߳���
//		idleThreadSize_++;    // ��¼��ʼ�����̵߳�����
//	}
//}
//
//// �����̺߳���   �̳߳ص������̴߳��������������������
//void ThreadPool::threadFunc(int threadid)  // �̺߳������أ���Ӧ���߳�Ҳ�ͽ�����
//{
//	auto lastTime = std::chrono::high_resolution_clock().now();
//
//	// �����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ
//	for (;;)
//	{
//		std::shared_ptr<Task> task;
//		{
//			// �Ȼ�ȡ��
//			std::unique_lock<std::mutex> lock(taskQueMtx_);
//
//			std::cout << "tid:" << std::this_thread::get_id()
//				<< "���Ի�ȡ����..." << std::endl;
//
//			// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳�
//			// �������յ�������initThreadSize_�������߳�Ҫ���л��գ�
//			// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60s
//
//			// ÿһ���з���һ��   ��ô���֣���ʱ���أ������������ִ�з���
//			// �� + ˫���ж�
//			while (taskQue_.size() == 0)
//			{
//				// �̳߳�Ҫ�����������߳���Դ
//				if (!isPoolRunning_)
//				{
//					threads_.erase(threadid); // std::this_thread::getid()
//					std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
//						<< std::endl;
//					exitCond_.notify_all();
//					return; // �̺߳����������߳̽���
//				}
//
//				if (poolMode_ == PoolMode::MODE_CACHED)
//				{
//					// ������������ʱ������
//					if (std::cv_status::timeout ==
//						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
//					{
//						auto now = std::chrono::high_resolution_clock().now();
//						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
//						if (dur.count() >= THREAD_MAX_IDLE_TIME
//							&& curThreadSize_ > initThreadSize_)
//						{
//							// ��ʼ���յ�ǰ�߳�
//							// ��¼�߳���������ر�����ֵ�޸�
//							// ���̶߳�����߳��б�������ɾ��   û�а취 threadFunc��=��thread����
//							// threadid => thread���� => ɾ��
//							threads_.erase(threadid); // std::this_thread::getid()
//							curThreadSize_--;
//							idleThreadSize_--;
//
//							std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
//								<< std::endl;
//							return;
//						}
//					}
//				}
//				else
//				{
//					// �ȴ�notEmpty����
//					notEmpty_.wait(lock);
//				}
//
//				//if (!isPoolRunning_)
//				//{
//				//	threads_.erase(threadid); // std::this_thread::getid()
//				//	std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
//				//		<< std::endl;
//				//	exitCond_.notify_all();
//				//	return; // �����̺߳��������ǽ�����ǰ�߳���!
//				//}
//			}
//
//			idleThreadSize_--;
//
//			std::cout << "tid:" << std::this_thread::get_id()
//				<< "��ȡ����ɹ�..." << std::endl;
//
//			// �����������ȡһ���������
//			task = taskQue_.front();
//			taskQue_.pop();
//			taskSize_--;
//
//			// �����Ȼ��ʣ�����񣬼���֪ͨ�������߳�ִ������
//			if (taskQue_.size() > 0)
//			{
//				notEmpty_.notify_all();
//			}
//
//			// ȡ��һ�����񣬽���֪ͨ��֪ͨ���Լ����ύ��������
//			notFull_.notify_all();
//		} // ��Ӧ�ð����ͷŵ�
//
//		// ��ǰ�̸߳���ִ���������
//		if (task != nullptr)
//		{
//			// task->run(); // ִ�����񣻰�����ķ���ֵsetVal��������Result
//			task->exec();
//		}
//
//		idleThreadSize_++;
//		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
//	}
//}
//
//bool ThreadPool::checkRunningState() const
//{
//	return isPoolRunning_;
//}
//
//////////////////  �̷߳���ʵ��
//int Thread::generateId_ = 0;
//
//// �̹߳���
//Thread::Thread(ThreadFunc func)
//	: func_(func)
//	, threadId_(generateId_++)
//{}
//
//// �߳�����
//Thread::~Thread() {}
//
//// �����߳�
//void Thread::start()
//{
//	// ����һ���߳���ִ��һ���̺߳��� pthread_create
//	std::thread t(func_, threadId_);  // C++11��˵ �̶߳���t  ���̺߳���func_
//	t.detach(); // ���÷����߳�   pthread_detach  pthread_t���óɷ����߳�
//}
//
//int Thread::getId()const
//{
//	return threadId_;
//}
//
//
///////////////////  Task����ʵ��
//Task::Task()
//	: result_(nullptr)
//{}
//
//void Task::exec()
//{
//	if (result_ != nullptr)
//	{
//		result_->setVal(run()); // ���﷢����̬����
//	}
//}
//
//void Task::setResult(Result* res)
//{
//	result_ = res;
//}
//
///////////////////   Result������ʵ��
//Result::Result(std::shared_ptr<Task> task, bool isValid)
//	: isValid_(isValid)
//	, task_(task)
//{
//	task_->setResult(this);
//}
//
//Any Result::get() // �û����õ�
//{
//	if (!isValid_)
//	{
//		return "";
//	}
//	sem_.wait(); // task�������û��ִ���꣬����������û����߳�
//	return std::move(any_);
//}
//
//void Result::setVal(Any any)  // ˭���õ��أ�����
//{
//	// �洢task�ķ���ֵ
//	this->any_ = std::move(any);
//	sem_.post(); // �Ѿ���ȡ������ķ���ֵ�������ź�����Դ
//}