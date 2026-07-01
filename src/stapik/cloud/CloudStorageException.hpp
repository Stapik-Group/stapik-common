#pragma once

#include <stdexcept>

class CloudStorageException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};