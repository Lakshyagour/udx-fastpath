#include <bits/stdc++.h>
using namespace std;


class TEMP {
  public:
  int print_mac_addr() {
    ifstream inputFile("/sys/class/net/UPF_TUN0/address");
    if (!inputFile.is_open()) {
      cerr << "Error opening the file!" << endl;
      return 1;
    }

    string address;
    cout << "File Content: " << endl;
    while (getline(inputFile, address)) {
      cout << address << endl; // Print the current line
    }
    inputFile.close();
    return 0;
  }
};
int main() { 
    
    TEMP t;
    t.print_mac_addr();
    return 0; }