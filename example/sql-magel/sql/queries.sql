-- queries.sql —— 示例项目在各个菜单里真正发给 MySQL 的查询（便于对照与手工验证）。
-- 占位符 <> 由 MAGEL 程序按用户输入替换。

-- 学生列表
SELECT no, name, gender, age, class_name FROM students ORDER BY no;

-- 按学号 / 按姓名查询学生
SELECT no, name, gender, age, class_name FROM students WHERE no = '<学号>';
SELECT no, name, gender, age, class_name FROM students WHERE name = '<姓名>';

-- 班级列表（人数直接取班级表里维护的值）
SELECT code, name, teacher, size FROM classes ORDER BY code;

-- 成绩列表：连表取姓名，用 CASE 算出等级
SELECT s.no, st.name, s.course, s.score,
       CASE WHEN s.score >= 90 THEN '优秀'
            WHEN s.score >= 80 THEN '良好'
            WHEN s.score >= 70 THEN '中等'
            WHEN s.score >= 60 THEN '及格'
            ELSE '不及格' END AS grade
FROM scores s LEFT JOIN students st ON st.no = s.no
ORDER BY s.no, s.course;

-- 成绩统计（按课程）
SELECT COUNT(*), AVG(score), MAX(score), MIN(score),
       SUM(CASE WHEN score >= 60 THEN 1 ELSE 0 END)
FROM scores WHERE course = '<课程>';

-- 最高分 / 最低分对应的学生
SELECT st.name FROM scores s JOIN students st ON st.no = s.no
WHERE s.course = '<课程>' ORDER BY s.score DESC LIMIT 1;
SELECT st.name FROM scores s JOIN students st ON st.no = s.no
WHERE s.course = '<课程>' ORDER BY s.score ASC LIMIT 1;
