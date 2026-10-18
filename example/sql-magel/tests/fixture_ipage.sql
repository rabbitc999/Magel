/*
 * fixture_ipage.sql —— ipage-student 信息管理系统的**自检夹具**（小规模、确定性）。
 *
 * 只用于 magel/example/sql-magel/tests/check-ipage.sh：建库脚本会先在独立测试库
 * `student_check` 里 DROP / CREATE 这五张表，再灌入固定数据，因此自增主键
 * 每次都从 1 开始，回答脚本里写死的 ID 才有确定含义。
 *
 * 表结构、外键行为、enum 取值与 ipage-student/student.sql 完全一致；
 * 数据只保留能覆盖全部分支的几条，不动真实库 `student`。
 */

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS `selection`;
DROP TABLE IF EXISTS `student`;
DROP TABLE IF EXISTS `sclass`;
DROP TABLE IF EXISTS `course`;
DROP TABLE IF EXISTS `user`;

CREATE TABLE `course`  (
  `course_id` int NOT NULL AUTO_INCREMENT,
  `course_name` varchar(36) NOT NULL,
  `course_state` enum('START','COMPLETION') NOT NULL,
  PRIMARY KEY (`course_id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

CREATE TABLE `sclass`  (
  `sclass_id` int NOT NULL AUTO_INCREMENT,
  `sclass_name` varchar(36) NOT NULL,
  `sclass_administrator` varchar(24) NOT NULL,
  `sclass_grade` enum('ONE','TWO','THREE') NULL,
  PRIMARY KEY (`sclass_id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

CREATE TABLE `student`  (
  `student_id` int NOT NULL AUTO_INCREMENT,
  `student_name` varchar(24) NOT NULL,
  `student_sex` enum('MALE','FEMALE') NOT NULL,
  `sclass_id` int NULL DEFAULT NULL,
  PRIMARY KEY (`student_id`) USING BTREE,
  INDEX `s_class_id`(`sclass_id` ASC) USING BTREE,
  CONSTRAINT `s_class_id` FOREIGN KEY (`sclass_id`) REFERENCES `sclass` (`sclass_id`) ON DELETE SET NULL ON UPDATE RESTRICT
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

CREATE TABLE `selection`  (
  `selection_id` int NOT NULL AUTO_INCREMENT,
  `student_id` int NOT NULL,
  `course_id` int NOT NULL,
  `test_date` date NOT NULL,
  `grade_number` bigint NOT NULL,
  PRIMARY KEY (`selection_id`) USING BTREE,
  INDEX `s_student_id`(`student_id` ASC) USING BTREE,
  INDEX `s_course_id`(`course_id` ASC) USING BTREE,
  CONSTRAINT `s_course_id` FOREIGN KEY (`course_id`) REFERENCES `course` (`course_id`) ON DELETE CASCADE ON UPDATE RESTRICT,
  CONSTRAINT `s_student_id` FOREIGN KEY (`student_id`) REFERENCES `student` (`student_id`) ON DELETE CASCADE ON UPDATE RESTRICT
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

CREATE TABLE `user`  (
  `user_id` int NOT NULL AUTO_INCREMENT,
  `user_name` varchar(18) NOT NULL,
  `user_password` varchar(18) NOT NULL,
  `user_permission` enum('TEACHER','STUDENT','ADMINISTRATOR','SUPER_ADMINISTRATOR') NOT NULL,
  PRIMARY KEY (`user_id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

INSERT INTO `user` VALUES (1, 'admin', '123456', 'SUPER_ADMINISTRATOR');
INSERT INTO `user` VALUES (2, '陈承岩', 'nahida', 'ADMINISTRATOR');
INSERT INTO `user` VALUES (3, '戚宇航', 'klee', 'TEACHER');
INSERT INTO `user` VALUES (4, '方智堃', 'zhongli', 'STUDENT');

INSERT INTO `sclass` VALUES (1, '1班', '高瑶', 'ONE');
INSERT INTO `sclass` VALUES (2, '2班', '杨茜', 'ONE');
INSERT INTO `sclass` VALUES (3, '1班', '孙茜怡', 'TWO');

INSERT INTO `course` VALUES (1, '语文', 'START');
INSERT INTO `course` VALUES (2, '数学', 'START');
INSERT INTO `course` VALUES (3, '英语', 'COMPLETION');

INSERT INTO `student` VALUES (1, '李航超', 'MALE', 1);
INSERT INTO `student` VALUES (2, '陈茜莉', 'FEMALE', 1);
INSERT INTO `student` VALUES (3, '孙永轩', 'MALE', 2);
INSERT INTO `student` VALUES (4, '陶婉萌', 'FEMALE', 3);
INSERT INTO `student` VALUES (5, '宇文莎', 'FEMALE', NULL);

INSERT INTO `selection` VALUES (1, 1, 1, '2025-06-01', 63);
INSERT INTO `selection` VALUES (2, 1, 2, '2025-06-01', 71);
INSERT INTO `selection` VALUES (3, 1, 3, '2025-06-01', 89);
INSERT INTO `selection` VALUES (4, 2, 1, '2025-06-01', 95);
INSERT INTO `selection` VALUES (5, 3, 2, '2025-06-01', 58);
INSERT INTO `selection` VALUES (6, 4, 1, '2025-06-01', 76);

SET FOREIGN_KEY_CHECKS = 1;
