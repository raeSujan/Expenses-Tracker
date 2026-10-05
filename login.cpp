#include <iostream>
#include <string>
#include <vector>

// Encapsulation: The User class wraps the data and behaviors of a single account.
class User {
private:
    std::string username;
    std::string password; // Kept private for data security

public:
    // Constructor to initialize a user
    User(std::string uname, std::string pword) {
        username = uname;
        password = pword;
    }

    // Getter for username
    std::string getUsername() const {
        return username;
    }

    // Secure validation method to hide the password from external access
    bool validateCredentials(const std::string& uname, const std::string& pword) const {
        return (username == uname && password == pword);
    }
};

// The UserManager class handles data management (Registration & Authentication)
class UserManager {
private:
    std::vector<User> database; // Mimics a user database collection

public:
    // Registers a unique user
    bool registerUser(const std::string& username, const std::string& password) {
        // Check if user already exists
        for (const auto& user : database) {
            if (user.getUsername() == username) {
                std::cout << "\n❌ Error: Username already exists!\n";
                return false;
            }
        }
        
        // Add new user to our simulation database
        database.push_back(User(username, password));
        std::cout << "\n✅ Registration successful!\n";
        return true;
    }

    // Handles the login logic
    bool loginUser(const std::string& username, const std::string& password) {
        for (const auto& user : database) {
            if (user.validateCredentials(username, password)) {
                std::cout << "\n🔓 Login successful! Welcome back, " << username << ".\n";
                return true;
            }
        }
        std::cout << "\n❌ Invalid username or password.\n";
        return false;
    }
};

int main() {
    UserManager system;
    int choice;
    std::string uname, pword;

    while (true) {
        std::cout << "\n=== IDENTITY PORTAL ===";
        std::cout << "\n1. Register";
        std::cout << "\n2. Login";
        std::cout << "\n3. Exit";
        std::cout << "\nSelect an option: ";
        std::cin >> choice;

        switch (choice) {
            case 1:
                std::cout << "Enter new username: ";
                std::cin >> uname;
                std::cout << "Enter new password: ";
                std::cin >> pword;
                system.registerUser(uname, pword);
                break;

            case 2:
                std::cout << "Enter username: ";
                std::cin >> uname;
                std::cout << "Enter password: ";
                std::cin >> pword;
                system.loginUser(uname, pword);
                break;

            case 3:
                std::cout << "\nExiting system. Goodbye!\n";
                return 0;

            default:
                std::cout << "\n Invalid choice. Try again.\n";
        }
    }
}
