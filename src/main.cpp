#include <iostream>
#include <fstream>

size_t fU(std::string_view str, char esc, std::string_view filter){
    for(size_t i = 0; i<str.length(); ++i){
        if(str[i]==esc){
            ++i;
            continue;
        }else if(filter.find(str[i])!=std::string::npos){
            return i;
        }
    }
    return std::string::npos;
}

std::string trim(std::string_view str, std::string_view tT){
    size_t s = str.find_first_not_of(tT);
    if(s==std::string::npos) return "";
    size_t e = str.find_last_not_of(tT);
    return std::string(str.substr(s, e-s+1));
}

std::string_view spaces = " \t\r\n";

int main(){
    std::ifstream f("test.ini");
    if(f.is_open()){
        std::string l;
        bool fL = true;
        while(std::getline(f, l)){
            if(fL){
                if(l.length() >= 3 && l.compare(0, 3, "\xEF\xBB\xBF")==0) l.erase(0,3);
                fL = false;
            }
            l = trim(l, spaces);
            size_t cmnt = fU(l, '\\', ";#");
            std::string dL = l.substr(0, cmnt);
            if(dL.empty()) continue;
            if(dL[0] == '['){
                size_t sE = fU(dL, '\\', "]");
                if(sE!=std::string::npos){
                    std::string section = dL.substr(1, sE-1);
                    std::cout << section << "\n";
                    continue;
                }
            }
            size_t eq = fU(dL, '\\', "=");
            std::string key;
            if(eq==0){
                key = "";
            }else{
                size_t kE = dL.find_last_not_of(spaces, eq-1);
                key = dL.substr(0, kE+1);
            }
            std::string value;
            if(eq==std::string::npos){
                value = "";
            }else{
                size_t vS = dL.find_first_not_of(spaces, eq+1);
                if(vS==std::string::npos){
                    value = "";
                }else{
                    size_t vE = dL.find_last_not_of(spaces);
                    value = dL.substr(vS, vE-vS+1);
                }
            }
//            std::cout << dL << "\n";
            std::cout << "KEY = " << key << "\n" << "VALUE = " << value << "\n\n";
        }
        f.close();
    }else{
        std::cerr << "Failed to open file.";
        return 1;
    }
    return 0;
}
