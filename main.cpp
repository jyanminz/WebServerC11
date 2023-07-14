#include<iostream>
#include<vector>
#include<string>

using namespace std;

int main(){
	vector<string> strs = {
		"test",
		"git",
		"push",
		"commit"
	};

	cout << "hello world" << endl;

	for (const auto & val : strs) {
		cout << val << endl;
	}

	return 0;
}
