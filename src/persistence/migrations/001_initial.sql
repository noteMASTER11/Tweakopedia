CREATE TABLE transactions (
    id TEXT PRIMARY KEY,
    package_name TEXT NOT NULL,
    status TEXT NOT NULL CHECK(status IN (
        'pending', 'running', 'succeeded', 'failed', 'rolled_back', 'interrupted'
    )),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    directory TEXT NOT NULL,
    error TEXT NOT NULL DEFAULT ''
);

CREATE INDEX transactions_created_at_idx ON transactions(created_at DESC);
