-- schema.sql —— 学生信息管理系统用到的三张表（原有表格的建表 SQL）。
--
-- 由 MAGEL 示例程序在启动时读入并交给外部 MySQL 执行；
-- 也可以手工执行：mysql -uroot -p123456 < sql/schema.sql

CREATE TABLE IF NOT EXISTS classes (
    code    VARCHAR(16)  NOT NULL,
    name    VARCHAR(32)  NOT NULL,
    teacher VARCHAR(32)  DEFAULT '',
    size    INT          NOT NULL DEFAULT 0,
    PRIMARY KEY (code)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS students (
    no         VARCHAR(16) NOT NULL,
    name       VARCHAR(32) NOT NULL,
    gender     VARCHAR(4)  DEFAULT '',
    age        INT         DEFAULT 0,
    class_name VARCHAR(32) DEFAULT '',
    PRIMARY KEY (no)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE IF NOT EXISTS scores (
    no     VARCHAR(16)  NOT NULL,
    course VARCHAR(32)  NOT NULL,
    score  DECIMAL(5,1) DEFAULT 0,
    PRIMARY KEY (no, course)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;
