#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

using namespace std;

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>
#include<thread>
// Any类型：可以接收任意数据的类型，也就是可以作为任意用户自定义任务的返回值类型
//也就是run()的返回值类型就是Any类型
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// 这个构造函数可以让Any类型接收任意其它的数据
	template<typename T>  // T:int    Derive<int>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}

	// 这个方法能把Any对象里面存储的data数据提取出来
	template<typename T>
	T cast_()
	{
		// 我们怎么从base_找到它所指向的Derive对象，从它里面取出data成员变量
		Derive<T>* pd = static_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}

private:
	// 基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// 派生类类型
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data)
		{}
		T data_;  // 保存了任意的其它类型，最终get方法是获取的这个数据
	};

private:
	// 定义一个基类的指针
	std::unique_ptr<Base> base_;
};

//信号量类，用于调用get()获取任务执行结果时，任务执行或没执行完的进程间的通信
class Seamphore
{
public:
	Seamphore(int limit = 0)
		:resLimit_(limit)
		,isExit_(false)

	{}
	~Seamphore()
	{
		isExit_=true;
	}

	//减少信号量资源计数
	void wait()
	{
		if(isExit_)
			return;

		std::unique_lock<std::mutex>lock(mtx_);
		while (resLimit_ <= 0)
			cond_.wait(lock);//没有资源计数的时候就阻塞等待
		resLimit_--;
	}

	//增加信号量资源计数
	void post()
	{
		if(isExit_)
			return;
		std::unique_lock<std::mutex>lock(mtx_);
		resLimit_++;
		cond_.notify_all();//通知所有条件变量，有资源计数了
	}

private:
	std::atomic_bool isExit_;
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Result;
//任务抽象基类
class Task
{
public:
	Task();//抽象基类用定义构造?
	~Task() = default;
	//用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	void exec();//通过exec调用run方法
	void setResult(Result* result);

	virtual Any run() = 0;
private:
	Result* result_;//这里要用裸指针，不能用智能指针，否则就会Task和Result之间发生智能指针的交叉引用问题
};


//线程池模式
//c++的枚举类，避免了不同枚举类中相同的枚举项冲突的问题，使用时需要加上PoolMode::
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};

//实现接收提交到线程池的task任务执行完成后的返回值类型Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isvalid = true);

	// 问题一: setVal方法，获取任务执行完的返回值的
	void setVal(Any any);

	// 问题二: get方法，用户调用这个方法获取task的返回值
	Any get();
private:
	Any any_;// 存储任务的返回值
	Seamphore sem_;// 线程通信信号量
	std::shared_ptr<Task> task_;// 指向对应获取返回值的任务对象
	std::atomic_bool isValid_;// 返回值是否有效
};


//线程类
class Thread
{
public:
	//线程函数对象类型，函数参数已经被bind绑定了，<>里就不用填参数类型了
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);

	~Thread();
	void start();

	//获取线程id
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_; //用于初始化线程id的值，从0开始，也能作为下标，用于释放map中的各个线程
							//可以理解成是用户自定义的，不要和thred::getid()弄混了
	int threadId_; // 保存线程id
};


//线程池类
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	//设置线程池工作模式
	void setMode(PoolMode mode);

	//设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(int threshhold);

	//设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshhold);

	//给线程池提交任务
	Result submitTask(std::shared_ptr<Task>sp);

	//开启线程池，同时指定线程池中的线程数量，为当前环境的CPU核心数量
	void start(int initThreadSize=std::thread::hardware_concurrency());

	//禁止线程池本身拷贝或者赋值，不符合逻辑
	ThreadPool(const ThreadPool& another) = delete;
	ThreadPool& operator=(const ThreadPool& another) = delete;

private:
	void threadFunc(int threadid);//把线程执行函数定义到ThreadPool中，方便访问ThreadPool中的各成员

	bool checkRunningState() const;//检查线程池的运行状态，只给线程池用，所以权限为private

private:
	//////////////线程池中的线程相关成员变量
	//std::vector<std::unique_ptr<Thread>> threads_; //线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;//线程列表，改用map，就可以通过对应的键找到对应的线程去释放
	size_t initThreadSize_; //初始线程数量
	int threadSizeThreshHold_;// 线程数量上限阈值
	std::atomic_int idleThreadSize_;// 记录线程池中空闲线程的数量
	std::atomic_int curThreadSize_;// 记录当前线程池中线程总数量

	//////////////任务队列相关成员变量
	std::queue<std::shared_ptr<Task>> taskQue_; //任务队列。这里要用智能指针
											//用户很有可能往任务队列中传递了一个临时对象，所以就不能用裸指针去指向这个临时的任务
											//因为会被自动析构，这个任务后续就无法被执行到了，应保证整个任务run完后再自动释放
	std::atomic_uint taskSize_; //任务的数量，使用原子类型
	int taskQueMaxThreshHold_; //任务队列数量上限阈值


	//////////////线程间通信相关成员变量
	std::mutex taskQueMtx_; //保证任务队列线程互斥的互斥锁

	//池类常用的一种思想，两个条件变量，分别给用户线程和线程池内部线程使用的，非满和非空
	std::condition_variable notFull_;//表示任务队列不满
	std::condition_variable notEmpty_;//表示任务队列不空
	std::condition_variable exitCond_;//等到线程资源全部回收


    //////////////线程池状态相关成员变量
	PoolMode poolMode_; //当前线程池的工作模式
	std::atomic_bool isPoolRunning_;//表示当前线程池的启动状态，是否启动。如果这个属性在多个线程中都会用到，那么应该设置成atomic类型
};
#endif
