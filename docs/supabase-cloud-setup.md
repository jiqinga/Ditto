# Supabase Cloud Relay Setup

Ditto's current cloud relay implementation talks to Supabase with the anon key over PostgREST. It does not create tables automatically yet, so the Supabase project must be initialized once before users can connect.

## One-time initialization

1. Open the target Supabase project.
2. Go to the SQL editor.
3. Run the SQL in `docs/supabase-init.sql`.
4. Copy the project URL and anon key into Ditto's Cloud Relay settings.
5. Enter a room code and test the connection again.

## What the SQL creates

- `public.sync_rooms`
- `public.sync_devices`
- `public.sync_messages`
- Row-level security policies required by the current anon-key client flow
- A trigger that updates `sync_devices.last_seen_at` whenever Ditto sends a heartbeat
- `timestamptz` values are stored in UTC on the server; convert them to local time only when displaying them

## Existing databases

If you already created the tables with an older draft of the SQL, update the defaults and heartbeat trigger to use `now()`:

```sql
alter table public.sync_rooms
  alter column created_at set default now();

alter table public.sync_devices
  alter column created_at set default now(),
  alter column last_seen_at set default now();

alter table public.sync_messages
  alter column created_at set default now();

create or replace function public.ditto_touch_sync_device()
returns trigger
language plpgsql
as $$
begin
    new.last_seen_at := now();
    return new;
end;
$$;
```

## Current limitation

Automatic schema bootstrap is not implemented in this branch yet. The client can detect the missing-schema case and now reports a targeted setup error, but the initial SQL still needs to be applied to the Supabase project one time.
