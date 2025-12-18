./fileserver_mt 8080 test_files 10 testpass &
SERVER_PID=$!
sleep 2

PASSED=0
FAILED=0

echo "Test 1: Authentication..."
echo "testpass" | ./fileclient localhost 8080 list > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  PASSED"
    ((PASSED++))
else
    echo "  FAILED"
    ((FAILED++))
fi

echo "Test 2: Upload file..."
echo "test" > test.txt
echo "testpass" | ./fileclient localhost 8080 upload test.txt > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  PASSED"
    ((PASSED++))
else
    echo "  FAILED"
    ((FAILED++))
fi

echo "Test 3: List files..."
echo "testpass" | ./fileclient localhost 8080 list | grep "test.txt"
if [ $? -eq 0 ]; then
    echo "  PASSED"
    ((PASSED++))
else
    echo "  FAILED"
    ((FAILED++))
fi

echo "Test 4: Download file..."
echo "testpass" | ./fileclient localhost 8080 download test.txt downloaded.txt > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  PASSED"
    ((PASSED++))
else
    echo "  FAILED"
    ((FAILED++))
fi

echo "Test 5: Delete file..."
echo "testpass" | ./fileclient localhost 8080 delete test.txt > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  PASSED"
    ((PASSED++))
else
    echo "  FAILED"
    ((FAILED++))
fi

kill $SERVER_PID
rm -f test.txt downloaded.txt
rm -rf test_files

echo "========================================="
echo "Results: $PASSED passed, $FAILED failed"
echo "========================================="

exit $FAILED