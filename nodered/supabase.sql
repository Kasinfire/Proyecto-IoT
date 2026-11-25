-- Tablas de la orquesta en Supabase. Se corre una sola vez en el SQL Editor del proyecto.
-- Solo se guarda el alias del niño (por ejemplo A-07): su nombre y las notas del terapeuta se
-- quedan en la laptop (nodered/almacen.js, paraNube).

create table if not exists public.ninos (
  alias  text primary key check (alias ~ '^[A-Z]{1,2}-[0-9]{2,3}$'),
  avatar jsonb,
  creado timestamptz not null default now()
);

create table if not exists public.sesiones (
  id      uuid primary key,
  alias   text not null references public.ninos (alias) on update cascade on delete cascade,
  inicio  timestamptz not null,
  fin     timestamptz not null,
  modos   text[] not null default '{}',
  cancion text,
  resumen jsonb not null,   -- gestos por animal, aciertos de Simón, segundos tocando juntos
  aviso   text,             -- por ejemplo "Pollito se desconectó"
  guardado timestamptz not null default now()
);

create index if not exists sesiones_alias_inicio on public.sesiones (alias, inicio desc);

-- RLS encendido y sin políticas: con la llave pública (anon/publishable) no se puede leer ni
-- escribir nada. Node-RED usa la llave secreta, que solo vive en .secrets/mqtt.env de la laptop.
alter table public.ninos enable row level security;
alter table public.sesiones enable row level security;

-- Permisos explícitos: algunos proyectos no se los dan solos a la llave secreta (error 42501).
-- Nunca darle esto a anon ni a authenticated.
grant usage on schema public to service_role;
grant select, insert, update, delete on public.ninos, public.sesiones to service_role;
