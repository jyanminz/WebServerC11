#pragma once

#include <mutex>
#include <deque>
#include <condition_variable>
#include <assert.h>
#include <sys/time.h>

template<typename T>
class BlockDeque {
public:
	explicit BlockDeque(size_t MaxCapacity = 1000);

	~BlockDeque();

	void clear();

	bool empty();

	bool full();

	void close();

	size_t size();

	size_t capacity();

	T front();

	T back();

	void push_back(const T & item);

	void push_front(const T & item);

	bool pop(T & item);

	bool pop(T & item, int timeout);
	
	void flush();

private:
	std::deque<T> deq_;

	size_t capacity_;

	std::mutex mtx_;

	bool IsClose_;

	std::condition_variable condConsumer_;

	std::condition_variable condProducer_;
};

template<typename T> 
BlockDeque<T>::BlockDeque(size_t MaxCapacity) : capacity_(MaxCapacity) {
	assert(MaxCapacity > 0);
	IsClose_ = false;
}

template<typename T>
BlockDeque<T>::~BlockDeque(){
	close();
}

template<typename T>
void BlockDeque<T>::close(){
	{
		std::lock_guard<std::mutex> locker(mtx_);
		deq_.clear();
		IsClose_ = true;
	}

	condProducer_.notify_all();
	condConsumer_.notify_all();
}

template<typename T>
void BlockDeque<T>::flush(){
	condConsumer_.notify_one();
}

template<typename T>
void BlockDeque<T>::clear(){
	std::lock_guard<std::mutex> locker(mtx_);
	deq_.clear();
}

template<typename T>
T BlockDeque<T>::front(){
	std::lock_guard<std::mutex> locker(mtx_);
	return deq_.front();
}

template<typename T>
T BlockDeque<T>::back(){
	std::lock_guard<std::mutex> locker(mtx_);
	return deq_.back();
}

template<typename T>
size_t BlockDeque<T>::size(){
	std::lock_guard<std::mutex> locker(mtx_);
	return deq_.size();
}

template<typename T>
size_t BlockDeque<T>::capacity(){
	std::lock_guard<std::mutex> locker(mtx_);
	return capacity_;
}

template<typename T>
void BlockDeque<T>::push_back(const T &item){
	std::unique_lock<std::mutex> locker(mtx_);
	while (deq_.size() >= capacity_) {
		condProducer_.wait(locker);
	}

	deq_.push_back(item);
	condConsumer_.notify_one();
}

template<typename T>
void BlockDeque<T>::push_front(const T& item){
	std::unique_lock<std::mutex> locker(mtx_);
	while (deq_.size() >= capacity_) {
		condProducer_.wait(locker);
	}

	deq_.push_front(item);
	condConsumer_.notify_one();
}

template<typename T>
bool BlockDeque<T>::empty() {
	std::lock_guard<std::mutex> locker(mtx_);
	return deq_.empty();
}

template<typename T>
bool BlockDeque<T>::full() {
	std::lock_guard<std::mutex> locker(mtx_);
	return deq_.size() >= capacity_;
}

template<typename T>
bool BlockDeque<T>::pop(T &item) {
	std::unique_lock<std::mutex> locker(mtx_);
	while (deq_.empty()) {
		condConsumer_.wait(locker);
		if(IsClose_) {
			return false;
		}
	}

	item = deq_.front();
	deq_.pop_front();
	condProducer_.notify_one();
	return true;
}

template<typename T>
bool BlockDeque<T>::pop(T &item,  int timeout) {
	std::unique_lock<std::mutex> locker(mtx_);
	while(deq_.empty()){
		if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout){
			return false;
		}
		if (IsClose_) {
			return false;
		}
	}
	item = deq_.front();
	deq_.pop_front();
	condProducer_.notify_one();
	return true;
}
