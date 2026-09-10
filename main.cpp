/*
    Employee Management System
    -----------------------------------
    A console based C++ project to manage employee records for a company.
    Uses OOP + basic DSA (vector, unordered_map, map, STL sort) with
    simple file handling so records survive between runs.

    Core idea: employees are stored in a vector (for ordering, sorting,
    iteration) and an unordered_map maps Employee ID -> vector index,
    so looking up a specific employee by ID does not require scanning
    the whole vector.
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <limits>
#include <cctype>
#include <cstdlib>

using namespace std;

const string DATA_FILE = "employees.txt";
const char DELIM = '|';   // using '|' instead of ',' so names/emails never break the format
const int FIELD_COUNT = 16;

// ===========================================================
// Small standalone helper functions
// (kept outside any class since they don't need employee data)
// ===========================================================

void clearInputError() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

string toLowerCase(const string& s) {
    string result = s;
    for (char& c : result) c = tolower((unsigned char)c);
    return result;
}

// Reads an integer within [minVal, maxVal], reprompting on bad input.
// Also cleans up cin so a getline() right after this works correctly.
int getValidatedInt(const string& prompt, int minVal, int maxVal) {
    int value;
    while (true) {
        cout << prompt;
        if (!(cin >> value)) {
            if (cin.eof()) {
                cout << "\nInput stream closed. Exiting.\n";
                exit(0);
            }
            cout << "Invalid input. Please enter a whole number.\n";
            clearInputError();
            continue;
        }
        if (value < minVal || value > maxVal) {
            cout << "Value must be between " << minVal << " and " << maxVal << ".\n";
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return value;
    }
}

double getValidatedDouble(const string& prompt, double minVal, double maxVal) {
    double value;
    while (true) {
        cout << prompt;
        if (!(cin >> value)) {
            if (cin.eof()) {
                cout << "\nInput stream closed. Exiting.\n";
                exit(0);
            }
            cout << "Invalid input. Please enter a number.\n";
            clearInputError();
            continue;
        }
        if (value < minVal || value > maxVal) {
            cout << "Value must be between " << minVal << " and " << maxVal << ".\n";
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return value;
    }
}

// Checks DD-MM-YYYY format and that the date is a real calendar date.
bool isValidDate(const string& date) {
    if (date.size() != 10) return false;
    if (date[2] != '-' || date[5] != '-') return false;
    for (int i = 0; i < 10; i++) {
        if (i == 2 || i == 5) continue;
        if (!isdigit((unsigned char)date[i])) return false;
    }

    int day = stoi(date.substr(0, 2));
    int month = stoi(date.substr(3, 2));
    int year = stoi(date.substr(6, 4));

    if (year < 1950 || year > 2026) return false;
    if (month < 1 || month > 12) return false;

    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    int maxDay = daysInMonth[month - 1];
    if (month == 2 && leapYear) maxDay = 29;

    if (day < 1 || day > maxDay) return false;
    return true;
}

// Rearranges DD-MM-YYYY into YYYYMMDD so plain string comparison sorts
// chronologically. Only call this on dates that already passed isValidDate().
string dateToComparableString(const string& date) {
    return date.substr(6, 4) + date.substr(3, 2) + date.substr(0, 2);
}

// Used only inside the sort comparator. Old data files (or a record that
// somehow slipped through) could contain a bad date - this falls back to
// a safe key instead of letting substr() throw and crash the program.
string safeDateKey(const string& date) {
    if (!isValidDate(date)) return "00000000";
    return dateToComparableString(date);
}

string getValidatedDate(const string& prompt) {
    string date;
    while (true) {
        cout << prompt;
        if (!getline(cin, date)) {
            cout << "\nInput stream closed. Exiting.\n";
            exit(0);
        }
        if (isValidDate(date)) return date;
        cout << "Invalid date. Use DD-MM-YYYY with a real calendar date.\n";
    }
}

// Case-insensitive match, returns the canonical spelling used for storage.
// Returns "" if the input doesn't match any allowed status.
string normalizeStatus(const string& input) {
    string lower = toLowerCase(input);
    if (lower == "active") return "Active";
    if (lower == "on leave") return "On Leave";
    if (lower == "resigned") return "Resigned";
    return "";
}

string getValidatedStatus(const string& prompt) {
    string result;
    while (true) {
        cout << prompt;
        string input;
        if (!getline(cin, input)) {
            cout << "\nInput stream closed. Exiting.\n";
            exit(0);
        }
        result = normalizeStatus(input);
        if (!result.empty()) return result;
        cout << "Invalid status. Enter one of: Active, On Leave, Resigned\n";
    }
}

string normalizeEmploymentType(const string& input) {
    string lower = toLowerCase(input);
    if (lower == "full-time" || lower == "fulltime") return "Full-Time";
    if (lower == "part-time" || lower == "parttime") return "Part-Time";
    if (lower == "intern") return "Intern";
    if (lower == "contract") return "Contract";
    return "";
}

string getValidatedEmploymentType(const string& prompt) {
    string result;
    while (true) {
        cout << prompt;
        string input;
        if (!getline(cin, input)) {
            cout << "\nInput stream closed. Exiting.\n";
            exit(0);
        }
        result = normalizeEmploymentType(input);
        if (!result.empty()) return result;
        cout << "Invalid type. Enter one of: Full-Time, Part-Time, Intern, Contract\n";
    }
}

// ---------------------------------------------------------
// Employee class
// ---------------------------------------------------------
class Employee {
public:
    int id;
    string name;
    int age;
    string gender;
    string phone;
    string email;
    string department;
    string designation;
    string dateOfJoining;     // stored as DD-MM-YYYY, kept as string for simplicity
    double salary;
    string employmentType;    // Full-Time / Part-Time / Intern / Contract
    string workLocation;
    string manager;
    string status;            // Active / On Leave / Resigned
    double experience;        // in years
    double performanceRating; // 1.0 to 5.0

    Employee() {}

    // Converts one employee record into a single '|' separated line for the file
    string toFileString() const {
        ostringstream oss;
        oss << id << DELIM << name << DELIM << age << DELIM << gender << DELIM
            << phone << DELIM << email << DELIM << department << DELIM << designation << DELIM
            << dateOfJoining << DELIM << salary << DELIM << employmentType << DELIM
            << workLocation << DELIM << manager << DELIM << status << DELIM
            << experience << DELIM << performanceRating;
        return oss.str();
    }

    // Rebuilds an Employee object from one line of the file.
    // Returns false (instead of throwing) if the line is malformed, so the
    // caller can skip the bad record without crashing the whole program.
    static bool fromFileString(const string& line, Employee& out) {
        stringstream ss(line);
        string token;
        vector<string> fields;

        while (getline(ss, token, DELIM)) {
            fields.push_back(token);
        }

        if ((int)fields.size() != FIELD_COUNT) {
            return false;
        }

        try {
            Employee e;
            e.id = stoi(fields[0]);
            e.name = fields[1];
            e.age = stoi(fields[2]);
            e.gender = fields[3];
            e.phone = fields[4];
            e.email = fields[5];
            e.department = fields[6];
            e.designation = fields[7];
            e.dateOfJoining = fields[8];
            e.salary = stod(fields[9]);
            e.employmentType = fields[10];
            e.workLocation = fields[11];
            e.manager = fields[12];
            e.status = fields[13];
            e.experience = stod(fields[14]);
            e.performanceRating = stod(fields[15]);
            out = e;
            return true;
        } catch (const exception&) {
            // stoi/stod throw on non-numeric text in a numeric field
            return false;
        }
    }

    void display() const {
        cout << "-----------------------------------------\n";
        cout << "Employee ID     : " << id << "\n";
        cout << "Name            : " << name << "\n";
        cout << "Age             : " << age << "\n";
        cout << "Gender          : " << gender << "\n";
        cout << "Phone           : " << phone << "\n";
        cout << "Email           : " << email << "\n";
        cout << "Department      : " << department << "\n";
        cout << "Designation     : " << designation << "\n";
        cout << "Date of Joining : " << dateOfJoining << "\n";
        cout << "Salary          : Rs. " << salary << "\n";
        cout << "Employment Type : " << employmentType << "\n";
        cout << "Work Location   : " << workLocation << "\n";
        cout << "Manager         : " << manager << "\n";
        cout << "Status          : " << status << "\n";
        cout << "Experience      : " << experience << " years\n";
        cout << "Performance     : " << performanceRating << " / 5\n";
    }
};

// ---------------------------------------------------------
// EmployeeManager class
// Handles all operations + owns the data structures
// ---------------------------------------------------------
class EmployeeManager {
private:
    vector<Employee> employees;            // main record store
    unordered_map<int, int> idToIndex;     // employee id -> position in the vector, for O(1) average lookup

    // Keeps idToIndex in sync with the vector. Must be called any time the
    // vector's element order or size changes in a way that isn't a simple
    // append (delete, sort). Costs O(n) since every id has to be re-mapped.
    void rebuildIndex() {
        idToIndex.clear();
        for (int i = 0; i < (int)employees.size(); i++) {
            idToIndex[employees[i].id] = i;
        }
    }

public:
    EmployeeManager() {
        loadFromFile();
    }

    // ---------- File handling ----------
    void loadFromFile() {
        ifstream fin(DATA_FILE);
        if (!fin.is_open()) {
            // no file yet, first run - nothing to load
            return;
        }

        string line;
        int lineNumber = 0;
        int skipped = 0;

        while (getline(fin, line)) {
            lineNumber++;
            if (line.empty()) continue;

            Employee e;
            if (Employee::fromFileString(line, e)) {
                employees.push_back(e);
            } else {
                skipped++;
                cout << "Warning: skipped invalid record on line " << lineNumber << " of " << DATA_FILE << "\n";
            }
        }
        fin.close();

        if (skipped > 0) {
            cout << skipped << " invalid record(s) were skipped while loading.\n";
        }
        rebuildIndex();
    }

    void saveToFile() {
        ofstream fout(DATA_FILE);
        for (const auto& e : employees) {
            fout << e.toFileString() << "\n";
        }
        fout.close();
    }

    // ---------- Core operations ----------

    // O(1) average - checks unordered_map instead of scanning the vector
    bool idExists(int id) {
        return idToIndex.find(id) != idToIndex.end();
    }

    void addEmployee() {
        int id;
        while (true) {
            id = getValidatedInt("Enter Employee ID: ", 1, numeric_limits<int>::max());
            if (idExists(id)) {
                cout << "An employee with this ID already exists. Enter a different ID.\n";
                continue;
            }
            break;
        }

        Employee e;
        e.id = id;

        cout << "Enter Name: "; getline(cin, e.name);
        e.age = getValidatedInt("Enter Age (18-65): ", 18, 65);
        cout << "Enter Gender: "; getline(cin, e.gender);
        cout << "Enter Phone Number: "; getline(cin, e.phone);
        cout << "Enter Email: "; getline(cin, e.email);
        cout << "Enter Department: "; getline(cin, e.department);
        cout << "Enter Designation: "; getline(cin, e.designation);
        e.dateOfJoining = getValidatedDate("Enter Date of Joining (DD-MM-YYYY): ");
        e.salary = getValidatedDouble("Enter Salary (greater than 0): ", 1, 100000000);
        e.employmentType = getValidatedEmploymentType("Enter Employment Type (Full-Time/Part-Time/Intern/Contract): ");
        cout << "Enter Work Location: "; getline(cin, e.workLocation);
        cout << "Enter Manager Name: "; getline(cin, e.manager);
        e.status = getValidatedStatus("Enter Status (Active/On Leave/Resigned): ");
        e.experience = getValidatedDouble("Enter Years of Experience (0-40): ", 0, 40);
        e.performanceRating = getValidatedDouble("Enter Performance Rating (1-5): ", 1, 5);

        employees.push_back(e);
        idToIndex[e.id] = employees.size() - 1;   // O(1), append only, no need to rebuild everything

        saveToFile();
        cout << "Employee added successfully.\n";
    }

    void displayAll() {
        if (employees.empty()) {
            cout << "No employee records found.\n";
            return;
        }
        cout << "\nTotal Employees: " << employees.size() << "\n";
        for (const auto& e : employees) {
            e.display();
        }
    }

    // O(1) average lookup using the id -> index map
    void searchById() {
        int id = getValidatedInt("Enter Employee ID to search: ", 1, numeric_limits<int>::max());

        auto it = idToIndex.find(id);
        if (it == idToIndex.end()) {
            cout << "No employee found with ID " << id << "\n";
            return;
        }
        employees[it->second].display();
    }

    // No index exists for department, so this is a linear O(n) scan.
    // Comparison is case-insensitive so "Engineering" / "engineering" / "ENGINEERING" all match.
    void searchByDepartment() {
        string dept;
        cout << "Enter Department: ";
        getline(cin, dept);
        string deptLower = toLowerCase(dept);

        bool found = false;
        for (const auto& e : employees) {
            if (toLowerCase(e.department) == deptLower) {
                e.display();
                found = true;
            }
        }
        if (!found) cout << "No employees found in department: " << dept << "\n";
    }

    void searchByDesignation() {
        string desig;
        cout << "Enter Designation: ";
        getline(cin, desig);
        string desigLower = toLowerCase(desig);

        bool found = false;
        for (const auto& e : employees) {
            if (toLowerCase(e.designation) == desigLower) {
                e.display();
                found = true;
            }
        }
        if (!found) cout << "No employees found with designation: " << desig << "\n";
    }

    void searchMenu() {
        cout << "\n--- Search Employee ---\n";
        cout << "1. By ID\n2. By Department\n3. By Designation\n4. Back to Main Menu\n";
        int choice = getValidatedInt("Enter choice: ", 1, 4);
        switch (choice) {
            case 1: searchById(); break;
            case 2: searchByDepartment(); break;
            case 3: searchByDesignation(); break;
            case 4: break;
        }
    }

    void updateEmployee() {
        int id = getValidatedInt("Enter Employee ID to update: ", 1, numeric_limits<int>::max());

        auto it = idToIndex.find(id);
        if (it == idToIndex.end()) {
            cout << "No employee found with ID " << id << "\n";
            return;
        }

        Employee& e = employees[it->second];
        cout << "\nWhat do you want to update?\n";
        cout << "1. Department\n2. Designation\n3. Salary\n4. Status\n5. Manager\n6. Performance Rating\n7. Phone\n8. Email\n";
        int choice = getValidatedInt("Enter choice: ", 1, 8);

        switch (choice) {
            case 1: cout << "New Department: "; getline(cin, e.department); break;
            case 2: cout << "New Designation: "; getline(cin, e.designation); break;
            case 3: e.salary = getValidatedDouble("New Salary (greater than 0): ", 1, 100000000); break;
            case 4: e.status = getValidatedStatus("New Status (Active/On Leave/Resigned): "); break;
            case 5: cout << "New Manager: "; getline(cin, e.manager); break;
            case 6: e.performanceRating = getValidatedDouble("New Performance Rating (1-5): ", 1, 5); break;
            case 7: cout << "New Phone: "; getline(cin, e.phone); break;
            case 8: cout << "New Email: "; getline(cin, e.email); break;
        }

        saveToFile();
        cout << "Employee updated successfully.\n";
    }

    // Removes an employee. Vector erase is O(n) because later elements shift left,
    // so the index map has to be rebuilt afterward - acceptable for this scale.
    void deleteEmployee() {
        int id = getValidatedInt("Enter Employee ID to delete: ", 1, numeric_limits<int>::max());

        auto it = idToIndex.find(id);
        if (it == idToIndex.end()) {
            cout << "No employee found with ID " << id << "\n";
            return;
        }

        employees.erase(employees.begin() + it->second);
        rebuildIndex();
        saveToFile();
        cout << "Employee deleted successfully.\n";
    }

    // Sorting menu - uses std::sort (introsort, O(n log n) average and worst case)
    void sortEmployees() {
        if (employees.empty()) {
            cout << "No records to sort.\n";
            return;
        }

        cout << "\nSort by:\n1. Salary (High to Low)\n2. Name (A-Z)\n3. Date of Joining\n4. Performance Rating (High to Low)\n";
        int choice = getValidatedInt("Enter choice: ", 1, 4);

        switch (choice) {
            case 1:
                sort(employees.begin(), employees.end(), [](const Employee& a, const Employee& b) {
                    return a.salary > b.salary;
                });
                break;
            case 2:
                sort(employees.begin(), employees.end(), [](const Employee& a, const Employee& b) {
                    return a.name < b.name;
                });
                break;
            case 3:
                // dates are stored as DD-MM-YYYY strings, so a plain string compare
                // won't sort chronologically - safeDateKey rearranges to YYYYMMDD
                // and never throws, even on a malformed legacy date.
                sort(employees.begin(), employees.end(), [](const Employee& a, const Employee& b) {
                    return safeDateKey(a.dateOfJoining) < safeDateKey(b.dateOfJoining);
                });
                break;
            case 4:
                sort(employees.begin(), employees.end(), [](const Employee& a, const Employee& b) {
                    return a.performanceRating > b.performanceRating;
                });
                break;
        }

        // sorting reorders the vector, so every index in the map is now stale
        rebuildIndex();
        saveToFile();
        cout << "Employees sorted successfully.\n";
        displayAll();
    }

    void highestLowestSalary() {
        if (employees.empty()) {
            cout << "No records available.\n";
            return;
        }

        auto maxIt = max_element(employees.begin(), employees.end(),
            [](const Employee& a, const Employee& b) { return a.salary < b.salary; });
        auto minIt = min_element(employees.begin(), employees.end(),
            [](const Employee& a, const Employee& b) { return a.salary < b.salary; });

        cout << "\nHighest Paid Employee:\n";
        maxIt->display();
        cout << "\nLowest Paid Employee:\n";
        minIt->display();
    }

    void filterByPerformance() {
        double minRating = getValidatedDouble("Show employees with rating >= : ", 1, 5);

        bool found = false;
        for (const auto& e : employees) {
            if (e.performanceRating >= minRating) {
                e.display();
                found = true;
            }
        }
        if (!found) cout << "No employees match that rating.\n";
    }

    void filterByStatus() {
        string status = getValidatedStatus("Enter Status to filter by (Active/On Leave/Resigned): ");

        bool found = false;
        for (const auto& e : employees) {
            if (e.status == status) {
                e.display();
                found = true;
            }
        }
        if (!found) cout << "No employees with status: " << status << "\n";
    }

    void filterMenu() {
        if (employees.empty()) {
            cout << "No records available.\n";
            return;
        }
        cout << "\n--- Filter Employees ---\n";
        cout << "1. By Minimum Performance Rating\n2. By Status\n3. Back to Main Menu\n";
        int choice = getValidatedInt("Enter choice: ", 1, 3);
        switch (choice) {
            case 1: filterByPerformance(); break;
            case 2: filterByStatus(); break;
            case 3: break;
        }
    }

    // Uses an ordered map so departments print alphabetically without an extra sort
    void departmentWiseCount() {
        map<string, int> deptCount;
        for (const auto& e : employees) {
            deptCount[e.department]++;
        }

        cout << "\nDepartment-wise Employee Count:\n";
        cout << "-----------------------------------------\n";
        for (const auto& pair : deptCount) {
            cout << pair.first << " : " << pair.second << "\n";
        }
    }

    void averageSalary() {
        double total = 0;
        for (const auto& e : employees) total += e.salary;
        cout << "\nAverage Salary: Rs. " << (total / employees.size()) << "\n";
    }

    void averagePerformance() {
        double total = 0;
        for (const auto& e : employees) total += e.performanceRating;
        cout << "\nAverage Performance Rating: " << (total / employees.size()) << " / 5\n";
    }

    void statusCount() {
        map<string, int> counts;
        for (const auto& e : employees) counts[e.status]++;

        cout << "\nEmployee Status Breakdown:\n";
        cout << "-----------------------------------------\n";
        cout << "Active     : " << counts["Active"] << "\n";
        cout << "On Leave   : " << counts["On Leave"] << "\n";
        cout << "Resigned   : " << counts["Resigned"] << "\n";
    }

    // For each department, tracks a running (total salary, employee count) pair
    // in one pass, then prints the average per department.
    void averageSalaryByDepartment() {
        map<string, pair<double, int>> deptTotals;
        for (const auto& e : employees) {
            deptTotals[e.department].first += e.salary;
            deptTotals[e.department].second += 1;
        }

        cout << "\nAverage Salary by Department:\n";
        cout << "-----------------------------------------\n";
        for (const auto& entry : deptTotals) {
            double avg = entry.second.first / entry.second.second;
            cout << entry.first << " : Rs. " << avg << "\n";
        }
    }

    void statisticsMenu() {
        if (employees.empty()) {
            cout << "No records available.\n";
            return;
        }

        int choice;
        do {
            cout << "\n--- Employee Statistics ---\n";
            cout << "1. Department-wise Count\n";
            cout << "2. Average Salary\n";
            cout << "3. Highest / Lowest Salary\n";
            cout << "4. Average Performance Rating\n";
            cout << "5. Active / On Leave / Resigned Count\n";
            cout << "6. Average Salary by Department\n";
            cout << "7. Back to Main Menu\n";
            choice = getValidatedInt("Enter choice: ", 1, 7);

            switch (choice) {
                case 1: departmentWiseCount(); break;
                case 2: averageSalary(); break;
                case 3: highestLowestSalary(); break;
                case 4: averagePerformance(); break;
                case 5: statusCount(); break;
                case 6: averageSalaryByDepartment(); break;
                case 7: break;
            }
        } while (choice != 7);
    }

    int totalEmployees() {
        return employees.size();
    }
};

// ---------------------------------------------------------
// Menu + main
// ---------------------------------------------------------
void printMainMenu() {
    cout << "\n===========================================\n";
    cout << "         EMPLOYEE MANAGEMENT SYSTEM\n";
    cout << "===========================================\n";
    cout << "1. Add Employee\n";
    cout << "2. Display All Employees\n";
    cout << "3. Search Employee\n";
    cout << "4. Update Employee\n";
    cout << "5. Delete Employee\n";
    cout << "6. Sort Employees\n";
    cout << "7. Employee Statistics\n";
    cout << "8. Filter Employees\n";
    cout << "9. Exit\n";
    cout << "===========================================\n";
}

int main() {
    EmployeeManager manager;

    cout << "Welcome to the Employee Management System\n";
    cout << manager.totalEmployees() << " employee record(s) loaded from file.\n";

    int choice;
    do {
        printMainMenu();
        choice = getValidatedInt("Enter your choice: ", 1, 9);

        switch (choice) {
            case 1: manager.addEmployee(); break;
            case 2: manager.displayAll(); break;
            case 3: manager.searchMenu(); break;
            case 4: manager.updateEmployee(); break;
            case 5: manager.deleteEmployee(); break;
            case 6: manager.sortEmployees(); break;
            case 7: manager.statisticsMenu(); break;
            case 8: manager.filterMenu(); break;
            case 9: cout << "Data saved. Exiting. Goodbye!\n"; break;
        }

    } while (choice != 9);

    return 0;
}
