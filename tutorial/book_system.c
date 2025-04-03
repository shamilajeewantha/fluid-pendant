#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define Book structure
typedef struct {
    char title[100];
    char author[100];
    int year;
} Book;

// Function to create a new book
Book* createBook(const char* title, const char* author, int year) {
    Book* b = (Book*)malloc(sizeof(Book));
    if (b == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);
    }
    strncpy(b->title, title, sizeof(b->title) - 1);
    strncpy(b->author, author, sizeof(b->author) - 1);
    b->year = year;
    return b;
}

// Function to print book details
void printBook(const Book* b) {
    if (b == NULL) return;
    printf("Title: %s\nAuthor: %s\nYear: %d\n", b->title, b->author, b->year);
}

// Function to free the book
void freeBook(Book* b) {
    free(b);
}

int main() {
    // Create a book
    Book* myBook = createBook("The C Programming Language", "Dennis Ritchie", 1978);

    printBook(myBook); // Print book details

    // Free memory
    freeBook(myBook);

    return 0;
}
