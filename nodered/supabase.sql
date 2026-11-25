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

-- Canciones: las incluidas (las trae el firmware del osito) y las que se crean en la página.
-- La melodía va como [[nota MIDI, pulsos], ...] y los acordes como lista por compás.
create table if not exists public.canciones (
  id       text primary key check (id ~ '^[a-z0-9-]{1,64}$'),
  nombre   text not null,
  tipo     text not null,
  estilo   text,
  compas   smallint,
  pulso_ms smallint not null,
  melodia  jsonb not null,
  acordes  jsonb,
  incluida boolean not null default false,
  guardado timestamptz not null default now()
);

alter table public.canciones enable row level security;

-- Historial: las sesiones como las muestra historial.html, para leerlas en el Table Editor.
-- security_invoker: la vista respeta el RLS de sesiones (la llave pública sigue sin ver nada).
create or replace view public.historial with (security_invoker = true) as
select
  s.id,
  s.alias,
  (s.inicio at time zone 'America/Mexico_City')::date as fecha,
  round(extract(epoch from (s.fin - s.inicio)) / 60)::int as minutos,
  s.modos,
  s.cancion,
  coalesce((s.resumen -> 'gestos' ->> 'ranita')::int, 0)
    + coalesce((s.resumen -> 'gestos' ->> 'gatito')::int, 0)
    + coalesce((s.resumen -> 'gestos' ->> 'pollito')::int, 0) as gestos,
  (s.resumen -> 'simon' ->> 'aciertos')::int as simon_aciertos,
  (s.resumen -> 'simon' ->> 'turnos')::int as simon_turnos,
  (s.resumen ->> 'juntos_s')::int as segundos_juntos,
  s.aviso,
  s.inicio
from public.sesiones s;

-- Permisos explícitos: algunos proyectos no se los dan solos a la llave secreta (error 42501).
-- Nunca darle esto a anon ni a authenticated.
grant usage on schema public to service_role;
grant select, insert, update, delete on public.ninos, public.sesiones, public.canciones to service_role;
grant select on public.historial to service_role;
revoke all on public.historial from anon, authenticated;
