#pragma once

#include<functional>
#include<future>
#include<mutex>
#include<queue>
#include<thread>
#include<utility>
#include<vector>

#include "SafeQueue.h"

class ThreadPool {
private:
	class ThreadWorker {
	private:
		int m_id;
		std::shared_ptr<ThreadPool> m_pool;
		
	public:
		ThreadWorker(ThreadPool *pool, const int id) : m_pool(std::make_shared<ThreadPool>(pool)), m_id(id){}

		void operator()(){
			std::function<void()> func;
			bool dequeued;

			while (!m_pool->IsClosed){
				{
					std::unique_lock<std::mutex> lock(m_pool->m_mtx);
					if (m_pool->m_queue.empty()) {
						m_pool->m_cond.wait(lock);
					}
					dequeued = m_pool->m_queue.dequeue(func);
				}
				if (dequeued) {
					func();
				}
			}
		}
	};

private:
	bool IsClosed;
	SafeQueue<std::function<void()>> m_queue;
	std::vector<std::thread> m_threads;
	std::mutex m_mtx;
	std::condition_variable m_cond;

public:
	ThreadPool(const int ThreadCounts = 8) : m_threads(std::vector<std::thread>(ThreadCounts)), IsClosed(false) {}

	ThreadPool(const ThreadPool &) = delete;
	
	ThreadPool(ThreadPool &&) = delete;

	void init() {
		for (int i = 0; i < m_threads.size(); i++) {
			m_threads[i] = std::thread(ThreadWorker(this, i));
		}
	}

	void shutdown() {
		IsClosed = true;
		m_cond.notify_all();

		for (int i = 0; i < m_threads.size(); i++) {
			if (m_threads[i].joinable()){
				m_threads[i].join();
			}
		}
	}

	template<typename F, typename...Args>
	auto AddTask(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
		std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

		auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

		std::function<void()> wrapper_func = [task_ptr](){
			(*task_ptr)();
		};

		m_queue.enqueue(wrapper_func);

		m_cond.notify_one();

		return task_ptr->get_future();
	}
};
