#include "heaptimer.h"
 
/* use small heap to handle timeout connections;
 */

void HeapTimer::siftup_(size_t i) {
	/* sift up
	 * if current node.expire time > its parent node.expire time
	 * swap the two nodes until current node.expires time is larger than its parent node.expires time
	 */

	assert(i >= 0 && heap_.size());

	size_t j = (i -1) / 2;
	while (j >= 0) {
		if (heap_[j] < heap_[i]) {
			break;
		}

		SwapNode_(i, j);
		i = j;
		j = (i-1) / 2;
	}
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
	/* sift down
	 * if current node.expire time > its child node.expire time 
	 * swap the nodes until current node.expires time is smaller than its left/right child node.expires time
	 */
	assert(index >= 0 && index < heap_.size());
	assert(n >= 0 && n <= heap_.size());

	// the current node
	size_t i = index;

	//the current node's child node
	size_t j = i * 2 + 1;

	while (j < n) {
		if (j + 1 < n && heap_[j+1] < heap_[j]) {
			// switch to another child node
			j++;
		}

		if (heap_[i] < heap_[j]) {
			// the current node.expires time is smaller than its child node.expires time
			// break
			break;
		}

		// else swap the two nodes
		SwapNode_(i, j);

		i = j;

		j = i * 2 + 1;
	}

	// if i > index meaning that 'swap' happens
	return i > index;
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
	if (i < 0 || i >= heap_.size()) {
		LOG_ERROR("SwapNode_.i:%d is out of range, heap_.size:%d", i, heap_.size());
		return;
	}

	if (j < 0 || j >= heap_.size()) {
		LOG_ERROR("SwapNode_.i:%d is out of range, heap_.size:%d", j, heap_.size());
		return;
	}
	// assert(i >= 0 && i < heap_.size());
	// assert(j >= 0 && j < heap_.size());
	
	// because we use file descriptor to locate the node in stored in the heap
	// thus swapNode not only has to change the value of the two nodes
	// but also the mapping relation between the two nods
	std::swap(heap_[i], heap_[j]);
	ref_[heap_[i].id] = i;
	ref_[heap_[j].id] = j;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
	if (id < 0) {
		LOG_ERROR("id:%d < 0", id);
		return;
	}

	size_t i;

	if (ref_.count(id) == 0) {
		// add to the bottom of the heap
		i = heap_.size();
		ref_[id] = i;
		heap_.push_back({id, Clock::now() + millisec(timeout), cb});
		// sift up to compare the current node.expires with its parent node.expires
		siftup_(i);
	}
	else {
		// the node is already exist in the heap
		// adjust the heap
		i = ref_[id];
		heap_[i].expires = Clock::now() + millisec(timeout);
		heap_[i].cb = cb;
		// compare the current node with its child's node 
		if (!siftdown_(i, heap_.size())) {
			// if 'sift_down' operation is conducted
			// compare the current node with its parent node
			siftup_(i);
		}
	}
}


void HeapTimer::doWork(int id) {
	/*
	 * trigger correspond callback function 
	 * and delete the current node
	 */

	if (heap_.empty() || ref_.count(id) == 0) {
		return;
	}

	size_t i = ref_[id];
	TimerNode node = heap_[i];
	node.cb();
	del_(i);
}

void HeapTimer::adjust(int id, int timeout) {
	assert(!heap_.empty() && ref_.count(id) > 0);
	heap_[ref_[id]].expires = Clock::now() + millisec(timeout);
	siftdown_(ref_[id], heap_.size());
}

void HeapTimer::del_(size_t index) {

	assert(!heap_.empty() && index >= 0 && index < heap_.size());

	size_t i = index;
	size_t n = heap_.size() - 1;

	assert(i <= n);

	/*
	 * for small heap, we always delete the top position node first
	 * thus we should push the assigned position node to the top
	 * then from the top to the bottom reconstructing the entire heap
	 */
	if (i < n) {
		SwapNode_(i, n);
		if (!siftdown_(i, n)) {
			siftup_(i);
		}
	}

	ref_.erase(heap_.back().id);
	heap_.pop_back();
}

void HeapTimer::tick() {
	if (heap_.empty()) {
		return;
	}

	while (!heap_.empty()) {
		TimerNode node = heap_.front();

		if (std::chrono::duration_cast<millisec>(node.expires - Clock::now()).count() > 0) {
			break;
		}

		node.cb();
		pop();
	}
}

void HeapTimer::pop() {
	assert(!heap_.empty());
	del_(0);
}

void HeapTimer::clear() {
	ref_.clear();
	heap_.clear();
}

int HeapTimer::GetNextTick() {

	tick();
	size_t res = -1;
	if (!heap_.empty()) {
		res = std::chrono::duration_cast<millisec>(heap_.front().expires - Clock::now()).count();
		if (res < 0) {
			res = 0;
		}
	}

	return res;
}
