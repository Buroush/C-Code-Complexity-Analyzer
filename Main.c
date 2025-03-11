#include <clang-c/Index.h>
#include <stdio.h>
#include <stdlib.h>

// Structure to hold complexity metrics and DOT generation data.
typedef struct {
    int cyclomatic;       // Cyclomatic complexity (start at 1)
    int varDeclCount;     // Count of variable declarations
    int currentLoopDepth; // Current loop nesting depth
    int maxLoopDepth;     // Maximum loop nesting depth encountered
    FILE *dotFile;        // File pointer for DOT output
    int nodeCounter;      // Unique node ID generator
    int nodeStack[1024];  // Stack to record parent node IDs for DOT graph
    int stackTop;         // Current stack depth
} Metrics;

// Visitor function to traverse the AST, update metrics, and generate DOT nodes/edges.
enum CXChildVisitResult visitor(CXCursor current, CXCursor parent, CXClientData client_data) {
    Metrics *metrics = (Metrics *)client_data;
    int nodeId = metrics->nodeCounter++;

    // Get a string for the current cursor (its spelling).
    CXString cursorSpelling = clang_getCursorSpelling(current);
    const char *label = clang_getCString(cursorSpelling);
    // Write the node with its label into the DOT file.
    fprintf(metrics->dotFile, "  node%d [label=\"%s\"];\n", nodeId, label);
    clang_disposeString(cursorSpelling);

    // If there's a parent on the stack, write an edge from parent to this node.
    if (metrics->stackTop > 0) {
        int parentId = metrics->nodeStack[metrics->stackTop - 1];
        fprintf(metrics->dotFile, "  node%d -> node%d;\n", parentId, nodeId);
    }
    
    // Push the current node ID onto the stack.
    metrics->nodeStack[metrics->stackTop++] = nodeId;

    // Update cyclomatic complexity for decision points.
    enum CXCursorKind kind = clang_getCursorKind(current);
    if (kind == CXCursor_IfStmt ||
        kind == CXCursor_ForStmt ||
        kind == CXCursor_WhileStmt ||
        kind == CXCursor_CaseStmt ||
        kind == CXCursor_ConditionalOperator) {
        metrics->cyclomatic++;
    }
    // Count variable declarations (as a simple indicator for space usage).
    if (kind == CXCursor_VarDecl) {
        metrics->varDeclCount++;
    }
    
    // If this node is a loop, update the loop nesting depth.
    int isLoop = (kind == CXCursor_ForStmt || kind == CXCursor_WhileStmt);
    if (isLoop) {
         metrics->currentLoopDepth++;
         if (metrics->currentLoopDepth > metrics->maxLoopDepth)
             metrics->maxLoopDepth = metrics->currentLoopDepth;
    }
    
    // Recursively visit children.
    clang_visitChildren(current, visitor, client_data);
    
    // After visiting children, if we were in a loop, decrement the loop depth.
    if (isLoop) {
         metrics->currentLoopDepth--;
    }
    
    // Pop the current node from the stack.
    metrics->stackTop--;
    
    return CXChildVisit_Continue;
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source-file.c>\n", argv[0]);
        return 1;
    }
    
    // Open the DOT file for writing the graphical AST representation.
    FILE *dotFile = fopen("ast.dot", "w");
    if (!dotFile) {
        perror("fopen");
        return 1;
    }
    fprintf(dotFile, "digraph G {\n");
    
    // Create an index and parse the source file.
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, argv[1], NULL, 0, NULL, 0, CXTranslationUnit_None);
    
    if (unit == NULL) {
        fprintf(stderr, "Unable to parse translation unit!\n");
        return 1;
    }
    
    CXCursor rootCursor = clang_getTranslationUnitCursor(unit);
    
    // Initialize metrics. Cyclomatic complexity starts at 1.
    Metrics metrics = {0};
    metrics.dotFile = dotFile;
    metrics.nodeCounter = 0;
    metrics.stackTop = 0;
    metrics.cyclomatic = 1;
    metrics.varDeclCount = 0;
    metrics.currentLoopDepth = 0;
    metrics.maxLoopDepth = 0;
    
    // Traverse the AST.
    clang_visitChildren(rootCursor, visitor, &metrics);
    
    // Finish the DOT file.
    fprintf(dotFile, "}\n");
    fclose(dotFile);
    
    // Convert the DOT file to an SVG file using Graphviz.
    if (system("dot -Tsvg ast.dot -o ast.svg") != 0) {
        fprintf(stderr, "Failed to convert DOT to SVG.\n");
    }
    
    // Use a shell command to launch the SVG, wait for user input, then delete the files.
    // This command opens the SVG, prints a prompt, waits for Enter, and removes ast.svg and ast.dot.
    if (system("sh -c 'xdg-open ast.svg && echo \"Press Enter to delete the SVG file\" && read dummy && rm ast.svg ast.dot'") != 0) {
        fprintf(stderr, "Failed to launch or delete the SVG file.\n");
    }
    
    // Output the computed metrics.
    printf("Cyclomatic Complexity: %d\n", metrics.cyclomatic);
    printf("Estimated Time Complexity: O(n^%d) based on max loop nesting depth\n", metrics.maxLoopDepth);
    printf("Estimated Space Complexity: O(n) with %d variable declarations\n", metrics.varDeclCount);
    
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    
    return 0;
}
