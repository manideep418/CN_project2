
#include "utils.h"

#include <iostream>

using namespace std;

void test_split()
{
	vector<string> result = split("google.com/query", '/');
	cout << result.size() << endl;
	for (int i = 0; i < result.size(); i++) {
		cout << result[i] << endl;
	}
}

void test_split_all()
{
	vector<string> result;
	result = split_all("google.com/query1/query2/query3", '/');
	cout << result.size() << endl;
	for (int i = 0; i < result.size(); i++) {
		cout << result[i] << endl;
	}

	result = split_all("google.com", '/');
	cout << result.size() << endl;
	for (int i = 0; i < result.size(); i++) {
		cout << result[i] << endl;
	}
}

int main()
{
	test_split();
	test_split_all();
}