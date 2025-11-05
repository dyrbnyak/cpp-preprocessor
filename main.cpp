#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool FindAndProcessInclude(const string& included_file_name, const path& file_path, ostream& out, const vector<path>& include_directories);
bool PreprocessRecursive(istream& src, ostream& out, const path& file_path, const vector<path>& include_directories);
bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);

//Вынес в отдельную функцию, тк она переиспользуется дважды 
bool FindAndProcessInclude(const string& included_file_name, const path& file_path, ostream& out, const vector<path>& include_directories, const int& count_string) {
    path included_file_path = file_path.parent_path() / included_file_name;

    //проверяем, есть ли файл, который мы проверяем.
    //если нету , проверяем файлы из переданной директории.
    if (!filesystem::exists(included_file_path)) {
        for (const auto& dir : include_directories) {
            included_file_path = dir / included_file_name;
            
            //Если директория нашлась, вызываем на неё рекурсию, чтоб приблизиться к рекурсивоному случаю
            if (filesystem::exists(included_file_path)) {
                ifstream file(included_file_path);
                return PreprocessRecursive(file, out, included_file_path, include_directories);

            }
        }

    //Если есть, "проваливаемся" в него
    } else {
        ifstream file(included_file_path);
        return PreprocessRecursive(file, out, included_file_path, include_directories);
    }

    //Если ничего не нашлось - выводим соотвествующую ошибку
    cout << "unknown include file " << included_file_name << " at file " << file_path.string() << " at line " << count_string << endl;
    return false;
}



bool PreprocessRecursive(istream& src, ostream& out, const path& file_path, const vector<path>& include_directories) {
    int count_string = 0;


    static regex incl1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/"); // для нахождения #include "..."
    static regex incl2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/"); // для нахождения #include <...>

    //Переменная, в которой хранится результат применения регулярного выражения.
    smatch m;
    string line;

    //Идем по файлу проверяя каждую строку
    while (getline(src, line)) {
        count_string++; 

        //ОБработка первого рекурсивного случая
        if (regex_match(line, m, incl1)) {
            //Забираем имя файла из include, обращайсь к группе 1, 0 - будет вся строка
            string included_file_name = m[1];
            
            //Передаем параметры в фунцию перебора диеркторий
            if (!FindAndProcessInclude(included_file_name, file_path, out, include_directories, count_string)) {
                return false;
            }

        //ОБработка второго рекурсивного случая
        } else if (regex_match(line, m, incl2)) {
            string included_file_name = m[1];
            if (!FindAndProcessInclude(included_file_name, file_path, out, include_directories, count_string)) {
                return false;
            }

        //ОБработка базового случая, когда подключения закончились. 
        } else {
            out << line << endl;

        }
    }
    return true;
}



bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream source_file(in_file);
    if (!source_file)
        return false;

    ofstream result_file(out_file);
    if (!result_file)
        return false;

    return PreprocessRecursive(source_file, result_file, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.txt"_p,
                        {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.txt"s) == test_out.str());
}

int main() {
    Test();
}
