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
// Any���ͣ����Խ����������ݵ����ͣ�Ҳ���ǿ�����Ϊ�����û��Զ�������ķ���ֵ����
//Ҳ����run()�ķ���ֵ���;���Any����
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// ������캯��������Any���ͽ�����������������
	template<typename T>  // T:int    Derive<int>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}

	// ��������ܰ�Any��������洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		// ������ô��base_�ҵ�����ָ���Derive���󣬴�������ȡ��data��Ա����
		Derive<T>* pd = static_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}

private:
	// ��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// ����������
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data)
		{}
		T data_;  // ������������������ͣ�����get�����ǻ�ȡ���������
	};

private:
	// ����һ�������ָ��
	std::unique_ptr<Base> base_;
};

//�ź����࣬���ڵ���get()��ȡ����ִ�н��ʱ������ִ�л�ûִ����Ľ��̼��ͨ��
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

	//�����ź�����Դ����
	void wait()
	{
		if(isExit_)
			return;

		std::unique_lock<std::mutex>lock(mtx_);
		while (resLimit_ <= 0)
			cond_.wait(lock);//û����Դ������ʱ��������ȴ�
		resLimit_--;
	}

	//�����ź�����Դ����
	void post()
	{
		if(isExit_)
			return;
		std::unique_lock<std::mutex>lock(mtx_);
		resLimit_++;
		cond_.notify_all();//֪ͨ������������������Դ������
	}

private:
	std::atomic_bool isExit_;
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

class Result;
//����������
class Task
{
public:
	Task();//��������ö��幹��?
	~Task() = default;
	//�û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	void exec();//ͨ��exec����run����
	void setResult(Result* result);

	virtual Any run() = 0;
private:
	Result* result_;//����Ҫ����ָ�룬����������ָ�룬����ͻ�Task��Result֮�䷢������ָ��Ľ�����������
};


//�̳߳�ģʽ
//c++��ö���࣬�����˲�ͬö��������ͬ��ö�����ͻ�����⣬ʹ��ʱ��Ҫ����PoolMode::
enum class PoolMode
{
	MODE_FIXED,
	MODE_CACHED,
};

//ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isvalid = true);

	// ����һ: setVal��������ȡ����ִ����ķ���ֵ��
	void setVal(Any any);

	// �����: get�������û��������������ȡtask�ķ���ֵ
	Any get();
private:
	Any any_;// �洢����ķ���ֵ
	Seamphore sem_;// �߳�ͨ���ź���
	std::shared_ptr<Task> task_;// ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;// ����ֵ�Ƿ���Ч
};


//�߳���
class Thread
{
public:
	//�̺߳����������ͣ����������Ѿ���bind���ˣ�<>��Ͳ��������������
	using ThreadFunc = std::function<void(int)>;

	Thread(ThreadFunc func);

	~Thread();
	void start();

	//��ȡ�߳�id
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_; //���ڳ�ʼ���߳�id��ֵ����0��ʼ��Ҳ����Ϊ�±꣬�����ͷ�map�еĸ����߳�
							//�����������û��Զ���ģ���Ҫ��thred::getid()Ū����
	int threadId_; // �����߳�id
};


//�̳߳���
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	//�����̳߳ع���ģʽ
	void setMode(PoolMode mode);

	//����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold);

	//�����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold);

	//���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task>sp);

	//�����̳߳أ�ͬʱָ���̳߳��е��߳�������Ϊ��ǰ������CPU��������
	void start(int initThreadSize=std::thread::hardware_concurrency());

	//��ֹ�̳߳ر��������߸�ֵ���������߼�
	ThreadPool(const ThreadPool& another) = delete;
	ThreadPool& operator=(const ThreadPool& another) = delete;

private:
	void threadFunc(int threadid);//���߳�ִ�к������嵽ThreadPool�У��������ThreadPool�еĸ���Ա

	bool checkRunningState() const;//����̳߳ص�����״̬��ֻ���̳߳��ã�����Ȩ��Ϊprivate

private:
	//////////////�̳߳��е��߳���س�Ա����
	//std::vector<std::unique_ptr<Thread>> threads_; //�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;//�߳��б�����map���Ϳ���ͨ����Ӧ�ļ��ҵ���Ӧ���߳�ȥ�ͷ�
	size_t initThreadSize_; //��ʼ�߳�����
	int threadSizeThreshHold_;// �߳�����������ֵ
	std::atomic_int idleThreadSize_;// ��¼�̳߳��п����̵߳�����
	std::atomic_int curThreadSize_;// ��¼��ǰ�̳߳����߳�������

	//////////////���������س�Ա����
	std::queue<std::shared_ptr<Task>> taskQue_; //������С�����Ҫ������ָ��
											//�û����п�������������д�����һ����ʱ�������ԾͲ�������ָ��ȥָ�������ʱ������
											//��Ϊ�ᱻ�Զ��������������������޷���ִ�е��ˣ�Ӧ��֤��������run������Զ��ͷ�
	std::atomic_uint taskSize_; //�����������ʹ��ԭ������
	int taskQueMaxThreshHold_; //�����������������ֵ


	//////////////�̼߳�ͨ����س�Ա����
	std::mutex taskQueMtx_; //��֤��������̻߳���Ļ�����

	//���ೣ�õ�һ��˼�룬���������������ֱ���û��̺߳��̳߳��ڲ��߳�ʹ�õģ������ͷǿ�
	std::condition_variable notFull_;//��ʾ������в���
	std::condition_variable notEmpty_;//��ʾ������в���
	std::condition_variable exitCond_;//�ȵ��߳���Դȫ������


    //////////////�̳߳�״̬��س�Ա����
	PoolMode poolMode_; //��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_;//��ʾ��ǰ�̳߳ص�����״̬���Ƿ������������������ڶ���߳��ж����õ�����ôӦ�����ó�atomic����
};
#endif
