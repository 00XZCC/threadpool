# threadpool
基于C++11实现的可跨平台使用的线程池

# 使用方式
class MyTask:public Task//自定义要执行的任务类，继承抽象基类Task
{
  Mytask(){...}
  Any run()//自定义Any类型，用于接收任务的执行结果
  {
    //...要执行的任务的逻辑
  }
}

int main()
{
  ThreadPool pool;//创建线程池对象
  pool.setMode(PoolMode::MODE_CACHED);//设置线程池模型，默认为fixed模式，cached模式需要手动设置
  pool.start();//启动线程池，可指定线程池中初始线程的个数，默认情况下为运行环境的CPU核心数

  Result result=pool.sumbmitTask(std::make_shared<myTask>(//...给自定义任务类的构造函数传参));//Result类对象result用于接收任意类型任务的执行结果
  
  auto t=result.get().cast_<T>();//获取任务执行的结果的值，值的类型可以为任意指定类型T类型
}
