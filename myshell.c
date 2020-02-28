/****************************************************************
 * Name        :  Piero Calenzani                               *
 * Class       :  CSC 415                                       *
 * Date        :  3/5/2019                                      *
 * Description :  Writting a simple bash shell program          *
 *                that will execute simple commands. The main   *
 *                goal of the assignment is working with        *
 *                fork, pipes and exec system calls.            *
 ****************************************************************/

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

// Max size of inputs to be read
#define BUFFERSIZE 256

// Prompt for each new shell input
#define PROMPT "myShell "
#define PROMPTSIZE sizeof(PROMPT)

// User home directory
#define HOME getenv("HOME")

// Delimeters for tokenizing
#define DELIM " \n\t"

/* COLORED TEXT CODES */
#define TXTCYAN "\x1B[36m"
#define TXTYELLOW "\x1B[33m"
#define TXTRESET "\x1B[0m"
/* --------------------*/

// Struct will hold each subcommand for executing
struct Command{
    // Array of strings tokenized from command input
    char** cmd;
    // Pointers to file descriptors, if applicable
    /* READ END (0) */
    int *readFrom;
    /* WRITE END (1) */
    int *writeTo;
};
// Wait flag for parent process
int WAITFLAG = 1;

void printPrompt();
char** parseStr(char[], int*, char*); // Input string tokenizer
void parseCmd(char**, int); // Split commands for piping
void execute(struct Command*);
void checkRedir(struct Command*);

int 
main(int argc, char** argv)
{
    // Initialize input buffer set to defined size
    char input[BUFFERSIZE];

    // Infinite loop will ask user for commands
    while(free)
    {
        WAITFLAG = 1;

        // Output prompt to console and get user input
        printPrompt();
        fgets(input, BUFFERSIZE, stdin);

        if (input[0] == '\n')
        {   // No input given
            continue;
        }

        // If command terminated with &
        if (input[strlen(input) - 2] == '&')
        {
            // Remove wait flag
            WAITFLAG = 0;
            // Remove & from command string
            input[strlen(input) - 2] = '\0';
        }

        // Piped commands count
        int myargc;
        // Array of piped commands
        char** myargv = parseStr(input, &myargc, "|");
        // Send array to parse and execute
        parseCmd(myargv, myargc);
    }

    return 0;
}

// Function will print prompt to the screen
void printPrompt()
{
    char path[256];
    char prompt[256];
    // Save current directory to path variable
    getcwd(path, sizeof(path));

    if (strstr(path, HOME) != NULL)
    {   // Home path found in current directory
        // Replace home path with '~'
        strcpy(prompt, "~");
        // Copy rest of path to prompt variable
        strcpy(&prompt[1], &path[strlen(HOME)]);
    }
    else
    {   // Not in home path
        // Copy path to prompt
        strcpy(prompt, path);
    }
    
    // Output prompt with current directory
    printf(TXTCYAN "%s" TXTYELLOW, PROMPT);
    printf("%s" TXTCYAN " >> " TXTRESET, prompt);

    return;
}

// Function will take in string input and pointer for token count
char** parseStr(char input[], int *count, char* delim)
{
    // Initialize counter and tokenizer
    int i = 0;
    char *p = strtok(input, delim);
    // 'String array' to contain all tokens
    char **tokens = NULL;

    while (p != NULL)
    {
        ++i;
        // Dynamically allocate memory as new tokens are parsed
        tokens = realloc(tokens, sizeof(char*) * i);

        // Error message if unable to allocate more memory
        if (tokens == NULL) {
            perror("\nMemory allocation failed");
            exit(-1);
        }

        // Append token to last index in array
        tokens[i - 1] = p;
        // Next token will start with NULL
        p = strtok(NULL, delim);
    }

    // Allocate space for one more token in array
    tokens = realloc(tokens, sizeof(char*) * (i+1) );
    if (tokens == NULL) {
        perror("\nMemory allocation failed");
        exit(-1);
    }
    // Last token will be null pointer
    tokens[i] = 0;

    // Set given pointer to token count
    *count = i;
    // Return array of strings
    return tokens;
}

// Function will take tokenized commands
void parseCmd(char** tokens, int count)
{
    // If no commands given, return NULL pointer
    if (count == 0) {return;}

    // If a single command is given
    if (count == 1)
    {
        // Copy new cmd string for tokenizing
        char cmdstr[256];
        strcpy(cmdstr, tokens[0]);

        int dummy = 0; // Dummy variable
        char** tcmd = parseStr(cmdstr, &dummy, DELIM);
        // Set cmd struct to tokenized command
        struct Command* cmd = malloc(sizeof (struct Command));
        cmd->cmd = tcmd;
        cmd->readFrom = NULL;
        cmd->writeTo = NULL;
        // Send struct to execute function
        execute(cmd);

        // Return single array of 1 command with no file descriptors
        return;
    }

    // Pipe set up
    int pipct = count - 1;
    int pipes[pipct][2];
    for (int i = 0; i < pipct; ++i)
    { // Fill array with pipes (1 between commands)
        if (pipe(pipes[i]) < 0)
        {   // Error check
            perror("Pipe failed");
        }
    }

    // Allocate memory for array of command structs
    struct Command *cmds = malloc(count * sizeof(struct Command));

    // For each token ...
    for (int i = 0; tokens[i] != NULL; ++i)
    {
        // Copy new cmd string for tokenizing
        char cmdstr[256];
        strcpy(cmdstr, tokens[i]);
        
        int dummy = 0; // Dummy variable
        char** tcmd = parseStr(cmdstr, &dummy, DELIM);
        // Set cmd struct to tokenized command
        cmds[i].cmd = tcmd;
        cmds[i].readFrom = NULL;
        cmds[i].writeTo = NULL;

        if (i > 0)
        {   // Not the first command
            // Set read file descriptor
            cmds[i].readFrom = malloc(sizeof(int*));
            *(cmds[i].readFrom) = pipes[i-1][0];
        }
        if (i != count - 1)
        {   // Not the last command
            // Set write file descriptor
            cmds[i].writeTo = malloc(sizeof(int*));
            *(cmds[i].writeTo) = pipes[i][1];
        }
        // Send struct in array to execute function
        execute(&cmds[i]);
    }
    
    return;
}

// Function will take Command struct as parameter and execute it
void execute(struct Command* mycmd)
{
    // Error check for null pointer
    if (mycmd == NULL) {return;}
    // Command sent to check function to resolve any redirections before execution
    checkRedir(mycmd);
    // Process ID of child for forking
    pid_t child;

    if ( (child = fork()) < 0)
    {   // Unable to create child process
        perror("Forking failed");
        exit(-1);
    }
    else if (child == 0)
    {   // Child Process
        if (mycmd->writeTo != NULL)
        {   // If command has redirected WRITE file descriptor
            // Duplicate to the STDOUT
            dup2( *(mycmd->writeTo), 1);
        }
        if (mycmd->readFrom != NULL)
        {   // If command has redirected READ file descriptor
            // Duplicate to the STDIN
            dup2( *(mycmd->readFrom), 0);
        }

        // Check if command is PWD for current directory
        if (strcmp(mycmd->cmd[0],"pwd") == 0)
        {
            // Set path variable and call getcwd function
            char path[256];
            getcwd(path, sizeof(path));
            /*  Function will write path output to the STDOUT
                If command overwrites the STDOUT, function will
                output to the overwrite */
            printf("%s\n", path);
            // Exit child process
            exit(0);
        }
        // Check if command is CD for changing directory
        else if (strcmp(mycmd->cmd[0],"cd") == 0)
        {
            if (strcmp(mycmd->cmd[1], "~") == 0)
            {   // If argument is '~'
                // Set argument to home directory
                mycmd->cmd[1] = getenv("HOME");
            }
            // Change directory to path (in argument)
            if (chdir(mycmd->cmd[1]) != 0)
            {   // Error check
                perror("chdir failed");
                exit(-1);
            }
        }

        // Close any open files
        if (mycmd->writeTo != NULL) {close( *(mycmd->writeTo) );}
        if (mycmd->readFrom != NULL) {close( *(mycmd->readFrom) );}
        // Call execvp function with current command line
        execvp(*(mycmd->cmd), mycmd->cmd);
    }   // END child
    else
    {   // Parent
        // Close any open files
        if (mycmd->writeTo != NULL) {close( *(mycmd->writeTo) );}
        if (mycmd->readFrom != NULL) {close( *(mycmd->readFrom) );}

        if (WAITFLAG)
        { // Wait flag is set
            int status;
            waitpid(child, &status, 0);
            if (status == 1)
            {
                perror("Child processing error.");
            }
        }
    }

    return;
}

// Function checks for redirect commands and sets file descriptors
void checkRedir(struct Command* mycmd)
{
    // Traverse through each member of cmd array
    for (int i = 0; mycmd->cmd[i] != NULL; ++i)
    {
        if (strcmp(mycmd->cmd[i],">") == 0)
        {   // If token matches '>'
            // Change token to NULL string, command will now end at previous token
            mycmd->cmd[i] = NULL;
            // Save file name from following token
            char* file = mycmd->cmd[i + 1];

            // Allocate memory for write end of struct
            mycmd->writeTo = malloc(sizeof(int*));
            if (mycmd->writeTo == NULL)
            {   // Error check
                perror("Memory Allocation failed");
                exit(-1);
            }

            // Open file with necessary redirect flags
            *(mycmd->writeTo) = open(file, O_WRONLY | O_CREAT | O_TRUNC);
            if ( *(mycmd->writeTo) < 0)
            {   // Error check
                perror("Error opening file");
                exit(-1);
            }
            return;
        }
        if (strcmp(mycmd->cmd[i],">>") == 0)
        {   // If token matches '>>'
            // Change token to NULL string, command will now end at previous token
            mycmd->cmd[i] = NULL;
            // Save file name from following token
            char* file = mycmd->cmd[i + 1];

            // Allocate memory for write end of struct
            mycmd->writeTo = malloc(sizeof(int*));
            if (mycmd->writeTo == NULL)
            {   // Error Check
                perror("Memory Allocation failed");
                exit(-1);
            }

            // Open file with necessary redirect flags
            *(mycmd->writeTo) = open(file, O_WRONLY | O_CREAT | O_APPEND);
            if ( *(mycmd->writeTo) < 0) 
            {   // Error Check
                perror("Error opening file");
                exit(-1);
            }
            return;
        }
        if (strcmp(mycmd->cmd[i],"<") == 0)
        {   // If token matches '<'
            // Change token to NULL string, command will now end at previous token
            mycmd->cmd[i] = NULL;
            // Save file name from following token
            char* file = mycmd->cmd[i + 1];

            // Allocate memory for read end of struct
            mycmd->readFrom = malloc(sizeof(int*));
            if (mycmd->readFrom == NULL)
            {   // Error check
                perror("Memory Allocation failed");
                exit(-1);
            }

            // Open file with necessary redirect flags
            *(mycmd->readFrom) = open(file, O_RDONLY);
            if ( *(mycmd->readFrom) < 0)
            {   // Error check
                perror("Error opening file");
                exit(-1);
            }
            return;
        }
    }

    // No redirects found
    return;
}