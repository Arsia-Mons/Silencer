# Private Route 53 zone shared by both boxes. Only resolves from inside
# the VPC. Holds A records for lobby + admin so cross-service hostnames
# stay stable across instance replacement (cloud-init reads e.g.
# admin.silencer.internal from /etc/silencer/*.env, never an IP).

resource "aws_route53_zone" "internal" {
  name = var.internal_zone_name

  vpc {
    vpc_id = data.aws_vpc.default.id
  }

  tags = {
    Name = "${var.project_name}-internal"
  }
}

resource "aws_route53_record" "lobby_internal" {
  zone_id = aws_route53_zone.internal.zone_id
  name    = "lobby.${var.internal_zone_name}"
  type    = "A"
  ttl     = 60
  records = [aws_instance.lobby.private_ip]
}

resource "aws_route53_record" "admin_internal" {
  zone_id = aws_route53_zone.internal.zone_id
  name    = "admin.${var.internal_zone_name}"
  type    = "A"
  ttl     = 60
  records = [aws_instance.admin.private_ip]
}
