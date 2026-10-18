-- seed.sql —— 初始演示数据（可重复执行，INSERT IGNORE 不会产生重复行）。

INSERT IGNORE INTO classes (code, name, teacher, size) VALUES
    ('C001', '计算机2401', '刘老师', 35),
    ('C002', '计算机2402', '陈老师', 32),
    ('C003', '软件2401',   '赵老师', 40);

INSERT IGNORE INTO students (no, name, gender, age, class_name) VALUES
    ('2024001', '张三', '男', 19, '计算机2401'),
    ('2024002', '李四', '女', 18, '计算机2402'),
    ('2024003', '王五', '男', 20, '计算机2401');

INSERT IGNORE INTO scores (no, course, score) VALUES
    ('2024001', 'Java',     92),
    ('2024001', '高等数学', 78),
    ('2024002', 'Java',     85);
