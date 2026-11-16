# Task Killer

A lightweight task manager for tracking daily tasks, reminders, and schedules with minimal complexity.

Task Killer is a simple productivity app designed to help users manage their tasks efficiently without the clutter of heavier productivity tools. It focuses on essential features like task scheduling, reminders, and deadline tracking.

## Features

- Create and manage tasks
- Set start times and deadlines
- Track task expiration
- Receive reminders and alerts
- View productivity statistics over time
- Lightweight desktop application

## Technologies

- C++
- GTK4
- libnotify
- Makefile

## Requirements

Before building this project, make sure you have:

- A C++17 compiler
- Make
- pkg-config
- GTK4 development libraries
- libnotify development libraries

## Build

```bash
git clone https://github.com/TerrenceAndrewDavis071118/simple-productivity-task-manager-.git
cd simple-productivity-task-manager-
make
```

Run the app:

```bash
./build/task-killer
```

## Testing

```bash
make test
```

## Project Structure

```text
.
├── src/
│   ├── main.cpp
│   ├── platform.hpp
│   ├── store.hpp
│   ├── style.hpp
│   └── audio.hpp
├── tests/
│   └── test_store_and_alerts.cpp
├── packaging/
├── assets/
├── Makefile
├── README.md
├── .gitignore
└── ...
```

## Notes

This project is focused on simplicity and usability for everyday task tracking and reminders. It is intended to be a fast and straightforward productivity tool rather than a feature-heavy planner.

## License

This repository does not currently include a license file. If you plan to share or distribute it publicly, consider adding an appropriate license such as MIT.
