#include"threadpool.h"
#include<iostream>
#include<chrono>
#include<thread>

using namespace std;

using uLong = unsigned long;

//在这里自定义要处理的任务的类
class myTask :public Task
{
public:
	myTask(int begin, int end)
		:begin_(begin)
		, end_(end)
	{}

	Any run()
	{
		int sum = 0;
		cout << "用户自定义的任务被执行了" << endl;
		//std::this_thread::sleep_for(std::chrono::seconds(10));
		for (int i = begin_; i <= end_; i++)
		{
			sum += i;
		}
		return sum;
	}

private:
	int begin_;
	int end_;
};

int main()
{
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);

		Result result1 = pool.submitTask(std::make_shared<myTask>(1, 1000000));
		Result result2 = pool.submitTask(std::make_shared<myTask>(1000001, 2000000));
		//Result result3 = pool.submitTask(std::make_shared<myTask>(2000001, 3000000)); 

		//uLong sum1 = result1.get().cast_<uLong>();
		//cout << sum1 << endl;
		//uLong sum2 = result2.get().cast_<uLong>();
		/*uLong sum3 = result1.get().cast_<uLong>();*/

		//cout << (sum1 + sum2) << endl;
	}

	//pool.start();

	getchar();
}