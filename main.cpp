#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iterator>
#include <sstream>
#include <ostream>
using namespace std;

vector<string> getline_csv(istream& str)
{
    vector<string> result;
    string line;
    getline(str,line);

    stringstream lineStream(line);
    string cell;
    while(getline(lineStream,cell, ','))
    {
        result.push_back(cell);
    }
    // This checks for a trailing comma with no data after it.
    if (!lineStream && cell.empty())
    {
        // If there was a trailing comma then add an empty element.
        result.push_back("");
    }
    return result;
}

int main() {
 ifstream in("ms_bucket.csv");
 auto title = getline_csv(in);
 for (auto &i : title) {
     cout << i << ' ';
 }
 cout << endl;
}

