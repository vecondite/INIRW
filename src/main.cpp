#include <iostream>
#include <fstream>
#include <list>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <filesystem>

template<typename kT, typename vT> class oMap{
    public:
        using Entry = std::pair<const kT, vT>;
        using DataList = std::list<Entry>;
        using EntryIterator = typename DataList::iterator;
        using CEntryIterator = typename DataList::const_iterator;
        using LocationMap = std::unordered_map<kT, EntryIterator>;
        using LocationIterator = typename LocationMap::iterator;
        using CLocationIterator = typename LocationMap::const_iterator;
    private:
        DataList data;
        LocationMap locs;
    public:
        oMap() = default;
        oMap(const oMap& other) : data(other.data) { for(EntryIterator it = data.begin(); it != data.end(); ++it) locs.emplace(it->first, it); }
        oMap& operator=(const oMap& other) {
            if(this!=&other){
                oMap temp(other);
                data.swap(temp.data);
                locs.swap(temp.locs);
            }
            return *this;
        }
        oMap(oMap&&) = default;
        oMap& operator=(oMap&&) = default;

        vT& operator[](const kT& key){
            LocationIterator val = locs.find(key);
            if(val!=locs.end()){
                return val->second->second;
            }else{
                EntryIterator newEntry = data.emplace(data.end(), key, vT{});
                locs.emplace(key, newEntry);
                return newEntry->second;
            }
        }
        void erase(const kT& key){
            LocationIterator val = locs.find(key);
            if(val!=locs.end()){
                data.erase(val->second);
                locs.erase(val);
            }
        }
        EntryIterator find(const kT& key) {
            LocationIterator val = locs.find(key);
            if(val!=locs.end()){
                return val->second;
            }else{
                return data.end();
            }
        }
        CEntryIterator find(const kT& key) const {
            CLocationIterator val = locs.find(key);
            if(val!=locs.end()){
                return val->second;
            }else{
                return data.end();
            }
        }
        EntryIterator begin() { return data.begin(); }
        EntryIterator end() { return data.end(); }
        CEntryIterator begin() const { return data.begin(); } 
        CEntryIterator end() const { return data.end(); }
        bool contains(const kT& key) const { return locs.find(key)!=locs.end(); }
        std::size_t size() const { return data.size(); }
        bool empty() const { return data.empty(); }
        void clear() { data.clear(); locs.clear(); }
};

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

std::string uesc(std::string_view str, char esc){
    std::string output = "";
    bool skip = false;
    for(char c : str){
        if(skip){
            skip = false;
            output += c;
        }else if(c == esc){
            skip = true;
        }else{
            output += c;
        }
    }
    return output;
}

std::string esc(std::string_view str, char escC, std::string_view filter){
    std::string output;
    for(char c : str){
        if(c == escC || filter.find(c)!=std::string::npos){
            output+=escC;
        }
        output+=c;
    } 
    return output;
}

constexpr std::string_view spaces = " \t\r\n";
constexpr std::string_view kEsc = ";#=[]";
constexpr std::string_view vEsc = ";#";
constexpr std::string_view sEsc = ";#]";

using Ini = oMap<std::string, oMap<std::string, std::string>>;
using SectData = oMap<std::string, std::string>;

void rwFile(std::string_view file, bool write, Ini& rWD){
    if(write && rWD.empty()){
        std::cerr << "Valid write data is required to write!\n";
        return;
    }
    Ini& wD = rWD;
    size_t instOffset = 0;
    std::vector<std::string> wL;
    std::fstream f;
    f.open(std::string{file}, std::ios::in | std::ios::binary);
    std::string section = "";
    bool pD = wD.find("")!=wD.end();
    bool crlf = false;
    unsigned int fL = 0;
    if(f.is_open()){
        std::string rL;
        while(std::getline(f, rL)){
            if(fL == 0){
                fL = 1;
                if(rL.length() >= 3 && rL.compare(0, 3, "\xEF\xBB\xBF")==0){
                    fL = 2;
                    rL.erase(0,3);
                }
                crlf = !rL.empty() && rL.back()=='\r';
            }
            std::string l = trim(rL, spaces);
            size_t cmnt = fU(l, '\\', vEsc);
            std::string dL = l.substr(0, cmnt);
            if(dL.empty()){
                if(write) wL.push_back(rL);
                continue;
            }  
            if(dL[0] == '['){
                size_t sE = fU(dL, '\\', "]");
                if(sE!=std::string::npos){
                    if(write){
                        Ini::EntryIterator rSect = wD.find(section);
                        if(rSect!=wD.end()){
                            for(const SectData::Entry& kV : rSect->second){
                                wL.emplace(wL.begin() + instOffset, esc(kV.first, '\\', kEsc) + " = " + esc(kV.second, '\\', vEsc));
                                ++instOffset;
                            }
                            wD.erase(rSect->first);
                        }
                    }
                    section = uesc(trim(dL.substr(1, sE-1),spaces), '\\');
                    if(write){
                        pD = wD.find(section)!=wD.end();
                        if(pD) wL.push_back(rL);
                    }else{
                        wD[section];
                    }
                    instOffset = wL.size();
                    continue;
                }
            }
            size_t eq = fU(dL, '\\', "=");
            std::string key = uesc(trim(dL.substr(0, eq), spaces), '\\');
            std::string value = eq==std::string::npos ? "" : uesc(trim(dL.substr(eq+1), spaces), '\\');
            cmnt = fU(rL, '\\', vEsc);
            dL = rL.substr(0, cmnt);
            eq = fU(dL, '\\', "=");
            size_t vS = eq == std::string::npos ? std::string::npos : dL.find_first_not_of(spaces, eq+1);
            size_t vE = dL.find_last_not_of(spaces);
            if(write){
            Ini::EntryIterator rSect = wD.find(section);
                if(rSect!=wD.end()){
                    SectData& sD = rSect->second;
                    if(pD){
                        SectData::EntryIterator found = sD.find(key);
                        if(found!=sD.end()){
                            if(found->second!=value){
                                std::string eVal = esc(found->second, '\\', vEsc);
                                if(eq==std::string::npos){
                                    rL.insert(vE+1, " = " + eVal);
                                }else if(vS==std::string::npos){
                                    rL.insert(eq+1, " " + eVal);
                                }else{
                                    rL.replace(vS, vE-vS+1, eVal);
                                }
                                wL.push_back(rL);
                            }else{
                                wL.push_back(rL);
                            }
                            instOffset = wL.size();
                            sD.erase(found->first);
                        }
                    }
                }
            }else{
                wD[section][key] = value;
            }
        }
        f.close();
    }else{
        if(!write) std::cerr << "Failed to open file: " << file << "\n";
    }
    if(write){
        Ini::EntryIterator lastSect = wD.find(section);
        if(lastSect!=wD.end()){
            for (const SectData::Entry& kV : lastSect->second){
                wL.emplace(wL.begin() + instOffset, esc(kV.first, '\\', kEsc) + " = " + esc(kV.second, '\\', vEsc));
                ++instOffset;
            }
            wD.erase(lastSect->first);
        }
        if(!wD.empty()){
            for(const Ini::Entry& rSect : wD){
                if(!rSect.first.empty()) wL.push_back("[" + esc(rSect.first, '\\', sEsc) + "]");
                for(const SectData::Entry& kV : rSect.second){
                    wL.push_back(esc(kV.first, '\\', kEsc) + " = " + esc(kV.second, '\\', vEsc));
                }
            }
        }
        std::string target{file};
        std::string tmp = target + ".tmp";
        std::error_code ec;
        f.open(tmp, std::ios::out | std::ios::trunc | std::ios::binary);
        if(!f.is_open()){
            std::cerr << "Error whilst opening " << tmp << " for writing!\n";
            return;
        }
        if(fL == 2) f << "\xEF\xBB\xBF";
        for(const std::string& wLD : wL){
            f << wLD << (crlf && (wLD.empty() || wLD.back()!='\r') ? "\r\n" : "\n");
        }
        f.close();
        if(f.fail()){    
            std::cerr << "Error whilst writing " << tmp << ", " << target << " left untouched!\n";
            std::filesystem::remove(tmp, ec);
            return;
        }
        std::filesystem::rename(tmp, target, ec);
        if(ec){
            std::cerr << "Could not replace " << target << ": " << ec.message() << "\n";
            std::filesystem::remove(tmp, ec);
        }
    }
}

void read(std::string_view file, Ini& output){
    output.clear();
    rwFile(file, false, output);
}

void write(std::string_view file, const Ini& data){
    Ini cD = data;
    rwFile(file, true, cD);
}

int main(){
    Ini output;
    read("test.ini", output);
    for(const auto& rSect : output){
        for(const auto& kV : rSect.second){
            std::cout << "Key: " << kV.first << " || " << "Value : " << kV.second << "\n";
        }
    }
    std::cout << "INITIAL READ DONE\n";
    output["section"]["key"] = "altered";
    output["section"]["added"] = "added";
    output["section2"]["key"] = "replaced";
    output["newsect"]["added2"] = "addedval";
    write("test.ini", output);
    return 0;
}
