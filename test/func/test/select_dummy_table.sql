-- selecting no columns
select 1 from foo;

-- simple select
select a, b from foo;

-- nonexistent columns
select a, c from foo;

-- mixing columns and literals
select 1, a, 2, b from foo;

-- renaming columns
select a as aa, b as bb from foo;

-- nonexistent tables
select 1 from bar;
