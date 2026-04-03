-- Ditto cloud relay schema bootstrap for Supabase
-- Run this once in the Supabase SQL editor for the target project.

create extension if not exists pgcrypto;

create table if not exists public.sync_rooms (
    id uuid primary key default gen_random_uuid(),
    lookup_key text not null unique,
    salt text not null,
    created_at timestamptz not null default now()
);

create table if not exists public.sync_devices (
    id uuid primary key default gen_random_uuid(),
    room_id uuid not null references public.sync_rooms(id) on delete cascade,
    device_name text not null,
    device_fingerprint text not null,
    last_seen_at timestamptz not null default now(),
    created_at timestamptz not null default now(),
    unique (room_id, device_name)
);

create index if not exists sync_devices_room_id_idx
    on public.sync_devices(room_id);

create table if not exists public.sync_messages (
    id uuid primary key default gen_random_uuid(),
    room_id uuid not null references public.sync_rooms(id) on delete cascade,
    sender_device_id text not null,
    payload_mode text not null,
    payload_inline text,
    payload_hash text,
    content_type text not null,
    created_at timestamptz not null default now()
);

create index if not exists sync_messages_room_created_idx
    on public.sync_messages(room_id, created_at, id);

create or replace function public.ditto_touch_sync_device()
returns trigger
language plpgsql
as $$
begin
    new.last_seen_at := now();
    return new;
end;
$$;

drop trigger if exists ditto_touch_sync_device on public.sync_devices;
create trigger ditto_touch_sync_device
before update on public.sync_devices
for each row
execute function public.ditto_touch_sync_device();

alter table public.sync_rooms enable row level security;
alter table public.sync_devices enable row level security;
alter table public.sync_messages enable row level security;

drop policy if exists "ditto_sync_rooms_select" on public.sync_rooms;
create policy "ditto_sync_rooms_select"
on public.sync_rooms
for select
using (true);

drop policy if exists "ditto_sync_rooms_insert" on public.sync_rooms;
create policy "ditto_sync_rooms_insert"
on public.sync_rooms
for insert
with check (true);

drop policy if exists "ditto_sync_devices_select" on public.sync_devices;
create policy "ditto_sync_devices_select"
on public.sync_devices
for select
using (true);

drop policy if exists "ditto_sync_devices_insert" on public.sync_devices;
create policy "ditto_sync_devices_insert"
on public.sync_devices
for insert
with check (true);

drop policy if exists "ditto_sync_devices_update" on public.sync_devices;
create policy "ditto_sync_devices_update"
on public.sync_devices
for update
using (true)
with check (true);

drop policy if exists "ditto_sync_messages_select" on public.sync_messages;
create policy "ditto_sync_messages_select"
on public.sync_messages
for select
using (true);

drop policy if exists "ditto_sync_messages_insert" on public.sync_messages;
create policy "ditto_sync_messages_insert"
on public.sync_messages
for insert
with check (true);
