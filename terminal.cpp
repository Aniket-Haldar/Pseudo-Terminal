


#include <iostream>      // For standard input and output
#include <unistd.h>      // For POSIX API: fork, read, write, dup2, close
#include <pty.h>         // For creating pseudo-terminals (openpty)
#include <termios.h>     // For terminal control settings
#include <sys/ioctl.h>   // For controlling terminal settings (ioctl)
#include <signal.h>      // For process control (setsid)
#include <poll.h>        // For monitoring multiple file descriptors (poll)

using namespace std;

int main() {
    int master_fd, slave_fd; // File descriptors for master and slave PTY
    char slave_name[256];    // Buffer to hold the name of the slave terminal

    // Create a pseudo-terminal (PTY) pair
    if (openpty(&master_fd, &slave_fd, slave_name, NULL, NULL) == -1) {
        perror("openpty");  // Print an error message if openpty fails
        return 1;           // Exit with error code 1
    }

    // Output the name of the slave terminal for debugging/informational purposes
    cout << "Slave terminal name: " << slave_name << endl;

    // Fork the current process to create a child
    pid_t pid = fork();
    if (pid == -1) { // Error during fork
        perror("fork");  // Print an error message
        return 1;        // Exit with error code 1
    }

    if (pid == 0) { // Child process
        close(master_fd); // Close the master PTY in the child process

        setsid(); // Create a new session and set the child process as its leader

        // Set the slave PTY as the controlling terminal for the child process
        if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
            perror("ioctl");  // Print an error message
            return 1;         // Exit with error code 1
        }

        // Redirect standard input, output, and error to the slave PTY
        dup2(slave_fd, STDIN_FILENO);   // Redirect stdin
        dup2(slave_fd, STDOUT_FILENO);  // Redirect stdout
        dup2(slave_fd, STDERR_FILENO);  // Redirect stderr

        close(slave_fd); // Close the slave PTY as it is no longer needed

        // Replace the child process with a Bash shell
        execlp("/bin/bash", "bash", nullptr); // Execute Bash
        perror("execlp");  // Print an error message if execlp fails
        return 1;          // Exit with error code 1
    } else { // Parent process
        close(slave_fd); // Close the slave PTY in the parent process

        // Set up polling to monitor file descriptors
        struct pollfd fds[2];
        fds[0].fd = master_fd;      // Monitor master PTY for child output
        fds[0].events = POLLIN;     // Interested in read events
        fds[1].fd = STDIN_FILENO;   // Monitor standard input for user input
        fds[1].events = POLLIN;     // Interested in read events

        char buffer[256]; // Buffer to hold data read from PTYs or stdin

        // Communication loop between the parent process and the Bash shell
        while (true) {
            // Wait for an event on either file descriptor
            int ret = poll(fds, 2, -1); // Block indefinitely until an event occurs
            if (ret == -1) { // Error during poll
                perror("poll");  // Print an error message
                break;           // Exit the loop
            }

            // Check if there is data to read from the master PTY (child process output)
            if (fds[0].revents & POLLIN) {
                ssize_t count = read(master_fd, buffer, sizeof(buffer) - 1);
                if (count > 0) { // If data is read successfully
                    buffer[count] = '\0'; // Null-terminate the buffer
                    cout << buffer;       // Output the data to the terminal
                }
            }

            // Check if there is data to read from stdin (user input)
            if (fds[1].revents & POLLIN) {
                ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                if (count > 0) { // If data is read successfully
                    buffer[count] = '\0'; // Null-terminate the buffer
                    write(master_fd, buffer, count); // Write the data to the child process
                }
            }
        }
    }

    return 0; // Exit the program successfully
}


